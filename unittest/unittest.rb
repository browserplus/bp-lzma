#!/usr/bin/env ruby

require File.join(File.dirname(File.dirname(File.expand_path(__FILE__))),
                  'external/dist/share/service_testing/bp_service_runner.rb')
require 'uri'
require 'test/unit'
require 'open-uri'
require 'rbconfig'
include Config
require 'base64'

# NEEDSWORK!!  DISABLED UNTIL WE FIGURE OUT HOW TO VALIDATE WITHOUT GEMS
# Neil (ndmanvar) wrote this test using gems:
# rubyzip gem
# tarruby gem
# ruby-lzma gem
#require 'rubygems'
#require 'zip/zip'
##require 'zip/ZipFileSystem'
#require 'lzma'

class TestLZMA < Test::Unit::TestCase
  def setup
    subdir = 'build/LZMA'
    if ENV.key?('BP_OUTPUT_DIR')
      subdir = ENV['BP_OUTPUT_DIR']
    end
    @cwd = File.dirname(File.expand_path(__FILE__))
    @service = File.join(@cwd, "../#{subdir}")
    @providerDir = File.expand_path(File.join(@cwd, "providerDir"))
    #@lzma = LZMA.new
  end
  
  def teardown
  end

  def test_load_service
    BrowserPlus.run(@service, @providerDir) { |s|
    }
  end

  def test_lzma_both_ways
    BrowserPlus.run(@service, @providerDir) { |s|
#      compressed = @lzma.compress("leroy was here")
#      assert_not_nil(compressed)
#      assert_equal("leroy was here", @lzma.decompress(compressed))
    }
  end
    
  def test_decompress_both_encoders
    BrowserPlus.run(@service, @providerDir) { |s|
#      java_compressed = Base64.decode64("XQAAQAAOAAAAAAAAAAA2GUqrt5owxRYShsh3Etc9r5jV/9OkAAA=")
#      assert_equal("leroy was here", @lzma.decompress(java_compressed))
#      native_compressed = Base64.decode64("EXQAAgAAOAAAAAAAAAAA2GUqrt5owxRYShsh3EtbQeg4A")
#      assert_equal("leroy was here", @lzma.decompress(java_compressed))
    }
  end

  def test_decompress_sample
    BrowserPlus.run(@service, @providerDir) { |s|
#      rakefile_compressed = Base64.decode64 %(XQAAgACYAwAAAAAAAAA5GUqKNoFH6SMqlNELWCfVN0/ayvP6n06cnR/N5xv3\noo9rE+JmQnElb8Tya7ikChUSRZBWc5ooDLDJ1se77Hg8CyqQP4MfTg9/vuCZ\n1tAYesF25sS0JKtrKxUF8shctO9Eme6EN6e9G79JIChQ1N2LKXetNnSvtkcS\ngxOP2L2owTeuCRZ1os6lZ/OC2a7mP/1dsxQLWAKU0H66gMtv8x54orKC31IC\nqjRJIe2RSpX31ZorKNxrnoCvt3bVyvF1fJ1XeSpxjJN+OwE9wAOtSRQaeC3b\njf1qODG+J068iIBpYbrEomcriVy5CqiZJdMiumBy2lQsrNHNjfcH4ggB678/\n2b1C2CYxOMOqZQTAz/TcsdjSBzSwSV//yJHP30gZT3H/7o9mn457OgV/l7KG\nAtwYMU/zcAxHWDpTX7V4H4v8YG75aJEfMAYiNcIlV9VU6xy8CvucboFiPIQv\nm7uzw4pnUwr+tKlH335EjovKU5qv+8ZNZaONQUxtnDaB76McQuigAA==\n)
#      decompressed = @lzma.decompress(rakefile_compressed)
#      assert_match(/require 'rake'/, decompressed)
#      assert_match(/desc "compile .classes to .jar"/, decompressed)
#      assert_match(/desc "compile C extension"/, decompressed)
#      assert_match(/task :test => :compile/, decompressed)
    }
  end

#  def test_compress_testfiletxt
#    BrowserPlus.run(@service, @providerDir) { |s|
#      output = s.compress({ 'file' => '/Users/ndmanvar/testfile.txt' })
#      assert_equal('.', @output[@output.length() - 3].chr)
#      assert_equal('l', @output[@output.length() - 2].chr)
#      assert_equal('z', @output[@output.length() - 1].chr)
#      assert_not_equal(0, @output.size())
#      #compressed = LZMA.compress('/Users/ndmanvar/testfile.txt')
#      #decompressed = LZMA.decompress(File.read("/Users/ndmanvar/Library/Caches/TemporaryItems/LZMAK6qFQS/0/testfile.txt.lz"))
#      #Zip::ZipFile.open(@output) { |zipfile| }
#    }
#  end
end
