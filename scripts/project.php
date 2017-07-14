<?php

/*

   bcc project management script, for Windows.

*/

define( 'PROJECT_DIR', dirname( __DIR__ ) );
define( 'SRC_DIR', PROJECT_DIR . '\\src' );
define( 'BUILD_DIR', PROJECT_DIR . '\\build' );
define( 'BUILD_X86_DIR', BUILD_DIR . '\\x86' );
define( 'BUILD_X64_DIR', BUILD_DIR . '\\x64' );
define( 'MAKEFILE_DIR', __DIR__ . '\\makefiles' );
define( 'MAKEFILE_POMAKE_DIR', MAKEFILE_DIR . '\\pomake' );
define( 'RELEASE_DIR', PROJECT_DIR . '\\releases' );
define( 'CONFIG_FILE', PROJECT_DIR . '\\config.php' );
define( 'EXIT_SUCCESS', 0 );
define( 'EXIT_FAILURE', 1 );
define( 'TARGET_X86', 0 );
define( 'TARGET_X64', 1 );
define( 'TARGET_DEFAULT', TARGET_X64 );
define( 'EXE_NAME', 'bcc.exe' );
define( 'MAKE_GNU', 0 );
define( 'MAKE_POMAKE', 1 );

class task {
   public $argv;
   public $script_path;
   public $config;
   public $command;
   public $command_args;

   public function __construct( $argv ) {
      $this->argv = $argv;
      $this->script_path = $argv[ 0 ];
      $this->config = new config();
      $this->command = '';
      $this->command_args = [];
   }
}

class config {
   public $make;
   public $strip_exe;

   public function __construct() {
      $this->make = MAKE_GNU;
      $this->strip_exe = false;
   }
}

function run( $argv ) {
   try {
      $task = new task( $argv );
      read_config( $task );
      read_command( $task );
      execute_command( $task );
      exit( EXIT_SUCCESS );
   }
   catch ( Exception $e ) {
      exit( EXIT_FAILURE );
   }
}

function read_config( $task ) {
   if ( file_exists( CONFIG_FILE ) ) {
      $config = $task->config;
      require_once CONFIG_FILE;
   }
}

function read_command( $task ) {
   $table = [
      'help',
      'make-all',
      'make-x86',
      'make-x64',
      'release',
      'create',
      'remove',
   ];
   if ( count( $task->argv ) >= 2 ) {
      $command = strtolower( $task->argv[ 1 ] );
      if ( in_array( $command, $table ) ) {
         $task->command = $command;
         $task->command_args = array_slice( $task->argv, 2 );
      }
   }
}

function execute_command( $task ) {
   $table = [
      'help' => 'handle_help',
      'make-all' => 'handle_make_all',
      'make-x86' => 'handle_make_x86',
      'make-x64' => 'handle_make_x64',
      'release' => 'handle_release',
      'create' => 'handle_create',
      'remove' => 'handle_remove',
      '' => 'handle_empty',
   ];
   if ( isset( $table[ $task->command ] ) ) {
      $table[ $task->command ]( $task );
   }
   else {
      show_err( "unhandled command: " . $task->command );
      bail();
   }
}

function handle_make_all( $task ) {
   compile_project( $task );
}

function compile_project( $task ) {
   compile_target( $task, TARGET_X86 );
   compile_target( $task, TARGET_X64 );
   copy_exe( TARGET_DEFAULT );
}

function compile_target( $task, $target ) {
   update_development_version_file( $task );
   make( $task, $target, $task->command_args );
}

function make( $task, $target, $args ) {
   switch ( $task->config->make ) {
   case MAKE_GNU:
      make_gnu( $task, $target, $args );
      break;
   case MAKE_POMAKE:
      make_pomake( $task, $target, $args );
      break;
   default:
      show_err( $task, 'invalid make option' );
      bail();
   }
}

function make_gnu( $task, $target, $args ) {
   $code = 1;
   $command = sprintf( 'make -I %s -f %s\\%s %s%s', MAKEFILE_DIR, MAKEFILE_DIR,
      $target == TARGET_X64 ? 'build_x64.mk' : 'build_x86.mk',
      $task->config->strip_exe ? ' STRIP_EXE=1' : '',
      implode( ' ', $args ) );
   system( $command, $code );
   if ( $code != 0 ) {
      show_err( $task, 'failed to execute make command' );
      bail();
   }
}

function make_pomake( $task, $target, $args ) {
   $code = 1;
   $command = sprintf( 'pomake /f %s\\%s %s%s', MAKEFILE_POMAKE_DIR,
      $target == TARGET_X64 ? 'build_x64.mk' : 'build_x86.mk',
      $task->config->strip_exe ? ' STRIP_EXE=1' : '',
      implode( ' ', $args ) );
   system( $command, $code );
   if ( $code != 0 ) {
      show_err( $task, 'failed to execute pomake command' );
      bail();
   }
}

function update_development_version_file( $task ) {
   system( '.\\version.bat dev' );
}

function handle_make_x86( $task ) {
   compile_target( $task, TARGET_X86 );
   copy_exe( TARGET_X86 );
}

function handle_make_x64( $task ) {
   compile_target( $task, TARGET_X64 );
   copy_exe( TARGET_X64 );
}

// Copies executable into project directory.
function copy_exe( $target ) {
   $src = sprintf( '%s\\%s', get_target_build_dir( $target ),
      EXE_NAME );
   $dst = sprintf( '%s\\%s', PROJECT_DIR, EXE_NAME );
   if ( file_exists( $src ) ) {
      copy( $src, $dst );
   }
}

function get_target_build_dir( $target ) {
   return ( $target == TARGET_X64 ? BUILD_X64_DIR : BUILD_X86_DIR );
}

function handle_help( $task ) {
   show_help( $task );
}

function show_help( $task ) {
   printf(
      "Usage: %s [command]\n" .
      "Commands:\n" .
      "  help             Show this help information\n" .
      "  make-all [args]  Execute make, passing it the specified arguments\n" .
      "  make-x86 [args]  Like make-all, but builds 32-bit binary only\n" .
      "  make-x64 [args]  Like make-all, but builds 64-bit binary only\n" .
      "  release          Compile project and package binaries\n" .
      "  create           Create build and release directories\n" .
      "  remove           Remove executable and build directory\n" .
      "",
      $task->script_path );
}

function handle_release( $task ) {
   compile_project( $task );
   create_release( $task );
}

function create_release( $task ) {
   create_target_release( $task, TARGET_X86 );
   create_target_release( $task, TARGET_X64 );
}

function create_target_release( $task, $target ) {
   $version = get_executable_version( $task, $target );
   $path = sprintf( '%s\\bcc-%s-%s.zip', RELEASE_DIR, $version,
      $target == TARGET_X64 ? '64bit' : '32bit' );
   $archive = new ZipArchive();
   $archive->open( $path, ZipArchive::CREATE );
   $exe = sprintf( '%s\\bcc.exe', $target == TARGET_X64 ?
      BUILD_X64_DIR : BUILD_X86_DIR );
   $archive->addFile( $exe, basename( $exe ) );
   $archive->addFile( 'lib/zcommon.bcs' );
   $archive->addFile( 'lib/zcommon.h.bcs' );
   $archive->addFile( 'lib/acs/README.txt' );
   $archive->addFile( 'doc/readme.txt', 'readme.txt' );
   $archive->close();
}

function get_executable_version( $task, $target ) {
   $output = [];
   $exe = sprintf( '%s\\%s', get_target_build_dir( $target ), EXE_NAME );
   $command = sprintf( '%s -version', $exe );
   exec( $command, $output );
   return $output[ 0 ];
}

function handle_create( $task ) {
   create_build_dir( $task );
   // Create release directory.
   if ( ! file_exists( RELEASE_DIR ) ) {
      mkdir( RELEASE_DIR );
   }
}

function create_build_dir( $task ) {
   if ( ! file_exists( BUILD_DIR ) ) {
      mkdir( BUILD_DIR );
      create_target_build_dir( $task, TARGET_X86 );
      create_target_build_dir( $task, TARGET_X64 );
   }
}

function create_target_build_dir( $task, $target ) {
   $build_dir = get_target_build_dir( $target );
   mkdir( $build_dir );
   mkdir( $build_dir . '\\parse' );
   mkdir( $build_dir . '\\parse\\token' );
   mkdir( $build_dir . '\\semantic' );
   mkdir( $build_dir . '\\codegen' );
   mkdir( $build_dir . '\\cache' );
}

function handle_remove( $task ) {
   remove_build_dir( $task );
   $exe = sprintf( '.\\%s', EXE_NAME );
   if ( file_exists( $exe ) ) {
      unlink( $exe );
   }
}

function remove_build_dir( $task ) {
   if ( file_exists( BUILD_DIR ) ) {
      remove_target_dir( $task, TARGET_X86 );
      remove_target_dir( $task, TARGET_X64 );
      // Remove development version file.
      system( 'php .\\scripts\\version.php remove' );
      rmdir( BUILD_DIR );
   }
}

function remove_target_dir( $task, $target ) {
   $build_dir = ( $target == TARGET_X64 ) ?
      BUILD_X64_DIR :
      BUILD_X86_DIR;
   if ( file_exists( $build_dir ) ) {
      // Remove executable.
      $exe = sprintf( '%s\\%s', $build_dir, EXE_NAME );
      if ( file_exists( $exe ) ) {
         unlink( $exe );
      }
      // Remove object files.
      $objects = get_object_file_paths( $task, $target );
      foreach ( $objects as $object ) {
         if ( file_exists( $object ) ) {
            unlink( $object );
         }
      }
      // Remove directories.
      rmdir( $build_dir . '\\cache' );
      rmdir( $build_dir . '\\codegen' );
      rmdir( $build_dir . '\\semantic' );
      rmdir( $build_dir . '\\parse\\token' );
      rmdir( $build_dir . '\\parse' );
      rmdir( $build_dir );
   }
}

function get_object_file_paths( $task, $target ) {
   $code = 1;
   $output = [];
   $command = sprintf( 'make -I %s -f %s\\%s show-objects', MAKEFILE_DIR,
      MAKEFILE_DIR, $target == TARGET_X64 ? 'build_x64.mk' : 'build_x86.mk' );
   exec( $command, $output, $code );
   if ( ! ( $code === 0 && count( $output ) == 1 ) ) {
      show_err( $task, 'failed to execute make command' );
      bail();
   }
   return explode( ' ', $output[ 0 ] );
}

function handle_empty( $task ) {
   show_help( $task );
   bail();
}

function show_err( $task, $msg = '' ) {
   $prefix = sprintf( '%s', basename( $task->script_path ) );
   if ( $msg == '' ) {
      $err = error_get_last();
      if ( $err !== null ) {
         $prefix = sprintf( '%s:%d', $prefix, $err[ 'line' ] );
         $msg = $err[ 'message' ];
      }
   }
   printf( "%s: error: %s\n", $prefix, $msg );
}

function bail() {
   throw new Exception();
}

run( $argv );
