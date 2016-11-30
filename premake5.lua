-- bcc compiler.

solution 'bcc'
   configurations 'release'
   language 'C'
   buildoptions {
      '-Wall',
      '-Werror',
      '-Wno-error=switch',
      '-Wno-unused',
      '-std=c99',
      '-pedantic',
      '-Wstrict-aliasing',
      '-Wstrict-aliasing=2',
      '-Wmissing-field-initializers',
      '-D_BSD_SOURCE',
      '-D_DEFAULT_SOURCE',
   }
   flags {
      'Symbols',
   }

   project 'bcc'
      location 'build'
      kind 'ConsoleApp'
      targetname 'bcc'
      targetdir '.'
      files {
         'src/*.c',
         'src/parse/*.c',
         'src/parse/token/*.c',
         'src/semantic/*.c',
         'src/codegen/*.c',
         'src/cache/*.c'
      }
      includedirs {
         'src',
         'src/parse',
      }
