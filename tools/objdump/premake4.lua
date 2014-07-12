solution 'objdump'
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
      '../../src/',
   }

   project 'objdump'
      location 'build'
      kind 'ConsoleApp'

      files {
         '*.c',
      }