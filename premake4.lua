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
      --'-pg',
   }

   linkoptions {
      --'-pg',
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
         'src/tools/ast_view.c',
      }

      excludes {
         'src/*.inc.c'
      }