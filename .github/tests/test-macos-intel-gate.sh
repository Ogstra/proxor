#!/usr/bin/env bash
set -euo pipefail

# Guard for the macOS Intel (x86_64, macOS 12) job and its OPTIONAL release gate (phase 59).
# - package-macos-intel builds with official Qt (aqtinstall) at deployment target 12.0 and checks arch + minos.
# - publish-release waits for it but never requires its success; bump-homebrew-tap cannot be skipped by it either.
# - MACOS_INTEL_REQUIRED is the single switch that makes the Intel zip mandatory later.
# - the arm64 job keeps its Homebrew Qt and macOS 15 floor.
# The checker proves it also fails for each way this could be quietly weakened (self-test mutations).
# Ruby 2.6 compatible (macOS system ruby): no endless methods, no pattern matching.

root="${1:-$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)}"

ruby -ryaml -e '
root = ARGV[0]
wf = YAML.load_file(File.join(root, ".github/workflows/build-proxor-cmake.yml"))

# Runner decision: 59-RESEARCH.md RUNNER=macos-15-intel (last GitHub Intel image; retires Fall 2027).
INTEL_RUNNER = "macos-15-intel"
SEVEN = %w[stage-package-source appimage package-windows package-deb package-rpm package-arch package-macos]

def violations(wf)
  out = []
  jobs = wf["jobs"] || {}
  arm = jobs["package-macos"]
  job = jobs["package-macos-intel"]
  if job.nil?
    out << "job package-macos-intel is missing"
  else
    steps = job["steps"] || []
    runs = steps.map { |s| s["run"].to_s }.join("\n")
    out << "package-macos-intel must be named Build macOS x86_64 app" unless job["name"] == "Build macOS x86_64 app"
    out << "package-macos-intel must run on #{INTEL_RUNNER}" unless job["runs-on"] == INTEL_RUNNER
    out << "package-macos-intel if: must equal package-macos" unless !arm.nil? && job["if"] == arm["if"]
    out << "package-macos-intel must need build-go" unless Array(job["needs"]).include?("build-go")
    env = job["env"] || {}
    out << "package-macos-intel MACOSX_DEPLOYMENT_TARGET must be 12.0" unless env["MACOSX_DEPLOYMENT_TARGET"].to_s == "12.0"
    qt = steps.find { |s| s["uses"].to_s.include?("install-qt-action") }
    if qt.nil?
      out << "package-macos-intel lacks the install-qt-action step"
    else
      w = qt["with"] || {}
      out << "install-qt-action version must be ${{ env.QT_VERSION }}" unless w["version"] == "${{ env.QT_VERSION }}"
      out << "install-qt-action host must be mac" unless w["host"] == "mac"
      out << "install-qt-action arch must be clang_64" unless w["arch"] == "clang_64"
    end
    out << "package-macos-intel must not use Homebrew Qt (brew install qtbase)" if runs.include?("brew install qtbase")
    out << "package-macos-intel must run ./libs/build_macos_intel.sh" unless runs.include?("./libs/build_macos_intel.sh")
    pkg = steps.find { |s| s["run"].to_s.include?("libs/package_macos.sh") }
    if pkg.nil?
      out << "package-macos-intel must run libs/package_macos.sh"
    else
      arch = (pkg["env"] || {})["PROXOR_MACOS_ARCH"] || env["PROXOR_MACOS_ARCH"]
      out << "package_macos.sh must run with PROXOR_MACOS_ARCH=x86_64" unless arch.to_s == "x86_64"
    end
    check = steps.find { |s| r = s["run"].to_s; r.include?("lipo") && r.include?("vtool") && r.include?("12.0") && r.include?("INTEL-ARCH-OK:") }
    out << "package-macos-intel lacks the lipo/vtool 12.0 INTEL-ARCH-OK: check step" if check.nil?
    cond = arm && (arm["steps"] || []).find { |s| (s["with"] || {})["name"] == "release-assets-macos" }
    tag_if = cond && cond["if"]
    rel = steps.find { |s| (s["with"] || {})["name"] == "release-assets-macos-intel" }
    if rel.nil?
      out << "package-macos-intel lacks the release-assets-macos-intel upload"
    else
      out << "release-assets-macos-intel path must be deployment/proxor-*-macos-x86_64.zip" unless rel["with"]["path"] == "deployment/proxor-*-macos-x86_64.zip"
      out << "release-assets-macos-intel must use the tag condition of package-macos" unless !tag_if.nil? && rel["if"] == tag_if
    end
    prev = steps.find { |s| (s["with"] || {})["name"] == "Proxor-${{ github.sha }}-macos-x86_64-preview" }
    if prev.nil?
      out << "package-macos-intel lacks the x86_64 preview upload"
    else
      out << "x86_64 preview upload must run when tag == empty" unless prev["if"].to_s.include?("github.event.inputs.tag == \x27\x27")
    end
    out << "package-macos-intel must not set job-level continue-on-error" if job.key?("continue-on-error")
  end
  out << "package-macos (arm64) must run on macos-15" unless arm && arm["runs-on"] == "macos-15"
  out << "package-macos (arm64) MACOSX_DEPLOYMENT_TARGET must stay 15.0" unless arm && (arm["env"] || {})["MACOSX_DEPLOYMENT_TARGET"].to_s == "15.0"
  arm_runs = arm ? (arm["steps"] || []).map { |s| s["run"].to_s }.join("\n") : ""
  out << "package-macos (arm64) must keep brew install qtbase" unless arm_runs.include?("brew install qtbase")
  out << "package-macos (arm64) must not use install-qt-action" if arm && (arm["steps"] || []).any? { |s| s["uses"].to_s.include?("install-qt-action") }
  out << "package-macos (arm64) must keep release-assets-macos with -macos-arm64.zip" unless arm && (arm["steps"] || []).any? { |s| (s["with"] || {})["name"] == "release-assets-macos" && s["with"]["path"].to_s.include?("-macos-arm64.zip") }

  # Optional gate (GATE_MECH=needs-result).
  pr = jobs["publish-release"]
  if pr.nil?
    out << "job publish-release is missing"
  else
    needs = Array(pr["needs"])
    (SEVEN + ["package-macos-intel"]).each { |n| out << "publish-release must need #{n}" unless needs.include?(n) }
    cond = pr["if"].to_s
    out << "publish-release if: must contain !cancelled()" unless cond.include?("!cancelled()")
    SEVEN.each { |n| out << "publish-release if: must require needs.#{n}.result == success" unless cond.include?("needs.#{n}.result == \x27success\x27") }
    out << "publish-release if: must not mention package-macos-intel" if cond.include?("package-macos-intel")
    pub = (pr["steps"] || []).find { |s| s["run"].to_s.include?("prepare-final-assets") }
    if pub.nil?
      out << "publish-release has no prepare-final-assets step"
    else
      r = pub["run"].to_s
      out << "publish step must reference --require-macos-intel" unless r.include?("--require-macos-intel")
      out << "publish step must test MACOS_INTEL_REQUIRED = y" unless r =~ /MACOS_INTEL_REQUIRED"? = y/
      out << "publish step must read MACOS_INTEL_REQUIRED from the workflow env" unless (pub["env"] || {})["MACOS_INTEL_REQUIRED"].to_s.include?("env.MACOS_INTEL_REQUIRED")
    end
  end
  out << "top-level env MACOS_INTEL_REQUIRED must exist and be n" unless (wf["env"] || {})["MACOS_INTEL_REQUIRED"].to_s == "n"
  bt = jobs["bump-homebrew-tap"]
  if bt.nil?
    out << "job bump-homebrew-tap is missing"
  else
    c = bt["if"].to_s
    out << "bump-homebrew-tap if: must contain !cancelled()" unless c.include?("!cancelled()")
    out << "bump-homebrew-tap if: must require needs.publish-release.result == success" unless c.include?("needs.publish-release.result == \x27success\x27")
    out << "bump-homebrew-tap if: must still exclude windows_only" unless c.include?("windows_only != \x27y\x27")
  end
  jobs.each { |name, j| out << "job #{name} must not have job-level continue-on-error" if j.key?("continue-on-error") }

  on = wf["on"] || wf[true] || {}
  inputs = ((on["workflow_dispatch"] || {})["inputs"] || {}).keys.sort
  out << "workflow_dispatch inputs changed: #{inputs.inspect}" unless inputs == %w[prerelease publish tag windows_only]
  out
end

found = violations(wf)
unless found.empty?
  warn "macOS Intel gate violations:"
  found.each { |m| warn "  #{m}" }
  exit 1
end

mutations = {
  "Intel job removed" => lambda { |w| w["jobs"].delete("package-macos-intel") },
  "Intel runs-on changed" => lambda { |w| w["jobs"]["package-macos-intel"]["runs-on"] = "macos-15" },
  "Intel deployment target 13.0" => lambda { |w| w["jobs"]["package-macos-intel"]["env"]["MACOSX_DEPLOYMENT_TARGET"] = "13.0" },
  "lipo/vtool step removed" => lambda { |w|
    j = w["jobs"]["package-macos-intel"]
    j["steps"] = j["steps"].reject { |s| s["run"].to_s.include?("vtool") }
  },
  "publish-release if requires the Intel job" => lambda { |w|
    j = w["jobs"]["publish-release"]
    j["if"] = j["if"].to_s + " && needs.package-macos-intel.result == \x27success\x27"
  },
  "Intel job dropped from publish-release needs" => lambda { |w|
    j = w["jobs"]["publish-release"]
    j["needs"] = Array(j["needs"]) - ["package-macos-intel"]
  },
  "MACOS_INTEL_REQUIRED removed" => lambda { |w| w["env"].delete("MACOS_INTEL_REQUIRED") },
  "arm64 job switched to install-qt-action" => lambda { |w|
    w["jobs"]["package-macos"]["steps"].each do |s|
      next unless s["run"].to_s.include?("brew install qtbase")
      s.delete("run")
      s["uses"] = "jurplel/install-qt-action@x"
    end
  },
  "bump-homebrew-tap if without needs.publish-release.result" => lambda { |w|
    j = w["jobs"]["bump-homebrew-tap"]
    j["if"] = j["if"].to_s.gsub("needs.publish-release.result == \x27success\x27", "true")
  },
  "Intel job got a job-level continue-on-error" => lambda { |w| w["jobs"]["package-macos-intel"]["continue-on-error"] = true },
  "publish step never passes --require-macos-intel" => lambda { |w|
    w["jobs"]["publish-release"]["steps"].each { |s| s["run"] = s["run"].gsub("--require-macos-intel", "") if s["run"] }
  },
  "dispatch input added" => lambda { |w|
    on = w["on"] || w[true]
    on["workflow_dispatch"]["inputs"]["intel"] = { "default" => "n" }
  },
}
mutations.each do |name, mutate|
  copy = Marshal.load(Marshal.dump(wf))
  mutate.call(copy)
  if violations(copy).empty?
    warn "self-test did not catch: #{name}"
    exit 1
  end
end

puts "macos-intel gate: OK"
' "$root"
