#!/usr/bin/env ruby

require 'common'

begin
    parseArgs()

    topDir = File.dirname(File.expand_path(__FILE__))

    if $rebuild
        puts "Removing previous build..."
        FileUtils.rm_rf("#{topDir}/#{$platform}")
    end

    externals = %w(easylzma)

    args = ""
    ARGV[0..ARGV.length].each do |arg|
        args += " " + arg
    end
    externals.each do |dir|
        Dir.chdir(dir) do
            puts ""
            puts "**************************************************"
            puts "Building #{dir}..."
            puts ""
            system("ruby build.rb #{args}")
        end 
    end

    if $clean
        puts "Removing previous build..."
        FileUtils.rm_rf("#{topDir}/#{$platform}")
    end
rescue RuntimeError => err
    puts "Error running build: " + err
end
