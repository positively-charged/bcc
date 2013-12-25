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
   }

   flags {
      'Symbols',
   }

   includedirs {
      'src/',
   }

   project 'bcc'
      location 'build'
      kind 'ConsoleApp'

      files {
         'src/*.c',
      }