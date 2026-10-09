#!/usr/bin/env bash
set -euo pipefail

# Release gate for the macOS in-app update: publish-release must need the "macOS update E2E" job and require its
# success, and that job must run the candidate update path (core download, relauncher suite, real bundle swap)
# on the E2E runner without any way to skip itself. The checker also proves it fails for each way the gate could
# be quietly weakened (self-test mutations). The runner label and the gate mode are the phase-60 decisions
# (E2E=ci:macos-15, E2E_GATE=publish), written here as constants so CI never reads .planning.

root="${1:-$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)}"

ruby -ryaml -e '
root = ARGV[0]
wf = YAML.load_file(File.join(root, ".github/workflows/build-proxor-cmake.yml"))

E2E_RUNNER = "macos-15"
E2E_GATE = "publish"
COND = "needs.update-e2e-macos.result == \x27success\x27"

def violations(wf)
  out = []
  jobs = wf["jobs"] || {}
  job = jobs["update-e2e-macos"]
  if job.nil?
    out << "job update-e2e-macos is missing"
  else
    out << "update-e2e-macos must be named macOS update E2E" unless job["name"] == "macOS update E2E"
    out << "update-e2e-macos must run on #{E2E_RUNNER}" unless job["runs-on"] == E2E_RUNNER
    pm = jobs["package-macos"]
    out << "package-macos is missing" if pm.nil?
    out << "update-e2e-macos must have the same if: as package-macos" if pm && job["if"] != pm["if"]
    out << "update-e2e-macos must need package-macos (the candidate zip)" unless Array(job["needs"]).include?("package-macos")
    out << "update-e2e-macos must not use job-level continue-on-error" if job.key?("continue-on-error")
    steps = job["steps"] || []
    runs = steps.map { |s| s["run"].to_s }.join("\n")
    ["PROXOR_MAC_UPDATE_E2E_REQUIRE=1", "go test ./go/grpc_server/ -run \x27TestUpdateE2EMac\x27",
     "bash packaging/macos/tests/test-app-update.sh", "Contents/Resources/update/proxor-app-update.sh",
     "MAC-UPDATE-E2E-OK"].each do |needle|
      out << "update-e2e-macos run steps lack: #{needle}" unless runs.include?(needle)
    end
    steps.each do |s|
      out << "update-e2e-macos step #{s["name"].inspect} must not use continue-on-error" if s.key?("continue-on-error")
    end
    [["Proxor-${{ github.sha }}-macos-arm64-preview", "github.event.inputs.tag == \x27\x27"],
     ["release-assets-macos", "github.event.inputs.tag != \x27\x27"]].each do |name, cond|
      ok = steps.any? do |s|
        s["uses"].to_s.include?("download-artifact") && (s["with"] || {})["name"] == name && s["if"].to_s.include?(cond)
      end
      out << "update-e2e-macos lacks a download-artifact step for #{name} with if: #{cond}" unless ok
    end
  end
  pr = jobs["publish-release"]
  if pr.nil?
    out << "publish-release is missing"
  elsif E2E_GATE == "publish"
    out << "publish-release must need update-e2e-macos" unless Array(pr["needs"]).include?("update-e2e-macos")
    out << "publish-release if: must require #{COND}" unless pr["if"].to_s.include?(COND)
  else
    out << "publish-release must not need update-e2e-macos (E2E_GATE=none)" if Array(pr["needs"]).include?("update-e2e-macos")
  end
  on = wf["on"] || wf[true] || {}
  inputs = ((on["workflow_dispatch"] || {})["inputs"] || {}).keys.sort
  expected = %w[prerelease publish tag windows_only]
  out << "workflow_dispatch inputs changed: #{inputs.inspect}" unless inputs == expected
  out
end

found = violations(wf)
unless found.empty?
  warn "mac-update e2e gate violations:"
  found.each { |m| warn "  #{m}" }
  exit 1
end

mutations = {
  "publish-release no longer needs update-e2e-macos" => lambda { |w|
    j = w["jobs"]["publish-release"]
    j["needs"] = Array(j["needs"]) - ["update-e2e-macos"]
  },
  "publish-release no longer requires the success" => lambda { |w|
    j = w["jobs"]["publish-release"]
    j["if"] = j["if"].gsub(" && " + COND, "")
  },
  "update-e2e-macos gained continue-on-error" => lambda { |w|
    w["jobs"]["update-e2e-macos"]["continue-on-error"] = true
  },
  "update-e2e-macos runs on ubuntu-latest" => lambda { |w|
    w["jobs"]["update-e2e-macos"]["runs-on"] = "ubuntu-latest"
  },
  "update-e2e-macos no longer requires the E2E" => lambda { |w|
    w["jobs"]["update-e2e-macos"]["steps"].each do |s|
      s["run"] = s["run"].gsub("PROXOR_MAC_UPDATE_E2E_REQUIRE=1", "") if s["run"]
    end
  },
  "update-e2e-macos if: changed" => lambda { |w|
    w["jobs"]["update-e2e-macos"]["if"] = "github.event_name == \x27pull_request\x27"
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

puts "mac-update e2e gate: OK"
' "$root"
