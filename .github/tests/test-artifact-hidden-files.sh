#!/usr/bin/env bash
set -euo pipefail

# From v4.4.0 on, upload-artifact skips dotfiles unless include-hidden-files is set.
# That silently dropped aur-release/.SRCINFO from an artifact whose other file still
# matched, so if-no-files-found never fired and the release failed forty minutes later
# at publish, on a file nobody had noticed was gone.
#
# A path that names a dotfile is easy to see. The trap is a path that names a directory:
# nothing in the workflow says what is inside it. So an upload whose path is not clearly
# a single file has to say out loud whether it wants hidden files, either way.

root="${1:-$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)}"

ruby -ryaml -e '
root = ARGV[0]
failures = []
checked = 0

Dir[File.join(root, ".github/workflows/*.yml")].sort.each do |file|
  workflow = YAML.load_file(file)
  (workflow["jobs"] || {}).each do |job_name, job|
    (job["steps"] || []).each do |step|
      next unless step["uses"].to_s.include?("upload-artifact")
      with = step["with"] || {}
      name = (with["name"] || step["name"] || "?").to_s
      paths = with["path"].to_s.split("\n").map(&:strip).reject(&:empty?)
      declared = with.key?("include-hidden-files")
      checked += 1

      paths.each do |path|
        component_is_hidden = path.split("/").any? { |c| c.start_with?(".") && c != "." && c != ".." }
        globbed = path.match?(/[*?\[]/)
        looks_like_file = File.extname(path) != ""

        if component_is_hidden && with["include-hidden-files"] != true
          failures << "#{File.basename(file)} / #{job_name} / #{name}: path #{path} names a hidden file but include-hidden-files is not true"
        elsif !globbed && !looks_like_file && !declared
          failures << "#{File.basename(file)} / #{job_name} / #{name}: path #{path} may be a directory, so include-hidden-files must be set explicitly, true or false"
        end
      end
    end
  end
end

abort("no upload-artifact step found -- this guard is looking in the wrong place") if checked.zero?

unless failures.empty?
  warn "upload-artifact steps that could silently drop a dotfile:"
  failures.each { |f| warn "  #{f}" }
  exit 1
end

puts "test-artifact-hidden-files.sh: #{checked} upload steps checked, all declare their intent"
' "$root"
