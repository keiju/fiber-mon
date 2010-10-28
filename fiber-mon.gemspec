
require "rubygems"

Gem::Specification.new do |s|
  s.name = "fiber-mon"
  s.authors = "Keiju.Ishitsuka"
  s.email = "keiju@ishitsuka.com"
  s.platform = Gem::Platform::RUBY
  s.summary = "Simple fiber scheduler for Ruby"
  s.rubyforge_project = s.name
  s.homepage = "http://github.com/keiju/fiber-mon"
  s.version = `git tag`.split.collect{|e| e.sub(/v([0-9]\.[0-9]\.[0-9]).*/, "\\1")}.sort.last
  s.require_path = "."
#  s.test_file = ""
#  s.executable = ""
  s.files = "fiber-mon.rb"
  s.description = <<EOF
Simple fiber scheduler for Ruby.
EOF
end

# Editor settings
# - Emacs -
# local variables:
# mode: Ruby
# end:
