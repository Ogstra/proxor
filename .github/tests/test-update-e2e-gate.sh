#!/usr/bin/env bash
set -euo pipefail

# Release gate for the in-app updater: package-windows (and through it publish-release) must need the
# "Windows update E2E" job, and that job must run the candidate updater, with no way to skip itself.
# The checker proves it also fails for each way the gate could be quietly weakened (self-test mutations).

root="${1:-$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)}"

ruby -ryaml -e '
root = ARGV[0]
wf = YAML.load_file(File.join(root, ".github/workflows/build-proxor-cmake.yml"))

def violations(wf)
  out = []
  jobs = wf["jobs"] || {}
  job = jobs["update-e2e-windows"]
  if job.nil?
    out << "job update-e2e-windows is missing"
  else
    out << "update-e2e-windows must run on windows-latest" unless job["runs-on"] == "windows-latest"
    out << "update-e2e-windows must not have an if: (it has to run on every push and dispatch)" if job.key?("if")
    out << "update-e2e-windows must need build-go (the candidate updater.exe)" unless Array(job["needs"]).include?("build-go")
    steps = job["steps"] || []
    runs = steps.map { |s| s["run"].to_s }.join("\n")
    ["PROXOR_UPDATE_E2E_REQUIRE=1", "PROXOR_UPDATE_E2E_UPDATER", "go test ./go/grpc_server/...", "go test ./go/cmd/updater/"].each do |needle|
      out << "update-e2e-windows run steps lack: #{needle}" unless runs.include?(needle)
    end
    runs.each_line do |line|
      if line.include?("go test ./go/grpc_server/") && line.include?(" -run ")
        out << "update-e2e-windows must run the whole grpc_server package, not a -run subset: #{line.strip}"
      end
    end
    want = "Proxor-${{ github.sha }}-Common-windows-amd64"
    has_artifact = steps.any? { |s| s["uses"].to_s.include?("download-artifact") && (s["with"] || {})["name"] == want }
    out << "update-e2e-windows lacks a download-artifact step for #{want}" unless has_artifact
  end
  pw = jobs["package-windows"]
  if pw.nil? || !Array(pw["needs"]).include?("update-e2e-windows")
    out << "package-windows must need update-e2e-windows"
  end
  pr = jobs["publish-release"]
  if pr.nil? || !Array(pr["needs"]).include?("package-windows")
    out << "publish-release must need package-windows"
  end
  on = wf["on"] || wf[true] || {}
  inputs = ((on["workflow_dispatch"] || {})["inputs"] || {}).keys.sort
  expected = %w[prerelease publish tag windows_only]
  out << "workflow_dispatch inputs changed: #{inputs.inspect}" unless inputs == expected
  out
end

found = violations(wf)
unless found.empty?
  warn "update E2E gate violations:"
  found.each { |m| warn "  #{m}" }
  exit 1
end

mutations = {
  "package-windows no longer needs update-e2e-windows" => lambda { |w|
    j = w["jobs"]["package-windows"]
    j["needs"] = Array(j["needs"]) - ["update-e2e-windows"]
  },
  "update-e2e-windows gained an if:" => lambda { |w|
    w["jobs"]["update-e2e-windows"]["if"] = "github.event_name == \"workflow_dispatch\""
  },
  "update-e2e-windows no longer requires the candidate updater" => lambda { |w|
    w["jobs"]["update-e2e-windows"]["steps"].each do |s|
      s["run"] = s["run"].gsub("PROXOR_UPDATE_E2E_REQUIRE=1", "") if s["run"]
    end
  },
  "update-e2e-windows runs on ubuntu-latest" => lambda { |w|
    w["jobs"]["update-e2e-windows"]["runs-on"] = "ubuntu-latest"
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

puts "update-e2e gate: OK"
' "$root"
