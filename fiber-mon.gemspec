
require "rubygems"

tag = `git tag`.split.sort.last
v, dmy, p = tag.scan(/^v([0-9]+\.[0-9]+\.[0-9]+)(-([0-9]+))?/).first
if p.to_i > 0
  v += "."+p
end
p v

Gem::Specification.new do |s|
  s.name = "fiber-mon"
  s.authors = "Keiju.Ishitsuka"
  s.email = "keiju@ishitsuka.com"
  s.platform = Gem::Platform::RUBY
  s.summary = "Simple fiber scheduler for Ruby"
  s.rubyforge_project = s.name
  s.homepage = "http://github.com/keiju/fiber-mon"
  s.version = v
  s.require_path = "."
#  s.test_file = ""
#  s.executable = ""
  s.files = ["fiber_mon.c", "fiber_mon.h", "xthread.h", "lib/fiber-mon.rb"]
  s.extensions = ["extconf.rb"]

  s.add_dependency("xthread", ">= 0.1.3")

  s.description = <<EOF
Simple fiber scheduler for Ruby.
EOF
end

# Editor settings
# - Emacs -
# local variables:
# mode: Ruby
# end:
