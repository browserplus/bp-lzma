#!/usr/bin/env ruby

require "bakery/ports/bakery"

# boost/zlib/bzip2 built from bakery port, bp-file is built from 
# submodule source
# libarchive will be put into ports/archivers/libarchive when 2.8.0 is released
topDir = File.dirname(File.expand_path(__FILE__));
$order = {
  :output_dir => File.join(topDir, "dist"),
  :packages => [
                "easylzma",
                "service_testing"
                ],
  :verbose => true,
  :use_source => {
        "bp-file"=>File.join(topDir, "bp-file")
   },
  :use_recipe => {
        "bp-file"=>File.join(topDir, "bp-file", "recipe.rb"),
   }
}

b = Bakery.new $order
b.build
