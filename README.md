Command line utilities to deal with MLV raw video files from Magic Lantern project. MLV (Magic Lantern Video) is a specialized container format from the open source project at http://www.magiclantern.fm
***
fpmutil : command line utility which generates pixel maps for getting rid of annoying focus pixel artifacts on some canon cameras.

Special thanks to Daniel Fort for initial hard work taken to study and analyze focus pixels to obtain their patterns and making script to generate maps. Project: https://bitbucket.org/daniel_fort/ml-focus-pixels


```

Focus Pixel Map Utility v1.0
****************************

Usage: ./fpmutil [options] [<inputfile1> <inputfile2> ...] [-o <outputfile>]
  -o <outputfile>           output filename with '.fpm' or '.pbm' extension
                            if omitted then name will be auto generated
Options:
  -c|--camera-name <name>   name: EOSM, 100D, 650D, 700D
  -m|--video-mode <mode>    mode: mv720      (1808x72* )
                                  mv1080     (1808x11**)
                                  mv1080crop (1872x10**)
                                  zoom       (2592x110*)
                                  croprec    (1808x72* ) 

  -u|--unified              switch to different, unified map generation mode
  -n|--no-header            do not include header into '.fpm' file
  -1|--one-pass-pbm         export multi pass '.fpm' as one pass '.pbm'
  -q|--quiet                supress console output
  -h|--help                 show long help

Notes:
  * auto generated name format is like used by MLVFS: 'cameraID_width_height.fpm'
  * multiple input files can be specified only for '.pbm' map images to save one combined multipass '.fpm'
  * to build crop_rec compliant maps using '.mlv' input, '-m croprec' should be used in conjunction with input file
  * if input file extension is '.fpm' or '.pbm' then conversion between input and output formats will be done
  * output map format will be chosen according to the file extension and if extension is wrong program will abort
  * if input '.mlv' from unsupportred camera the warning will be shown and program will abort
  * if '-u' switch specified, will export unified, aggresive pixel map to fix restricted to 8-12bit lossless raw
  * if '-n' switch specified, will export '.fpm' without header
  * if '-1' switch specified, will export all passes in one .pbm, by default separate file created for each pass

Examples:
  fpmutil -c EOSM -m mv1080                     will save '.fpm' 1808x1190 map with auto generated name
  fpmutil -n -c EOSM -m mv1080                  will save '.fpm' 1808x1190 map with auto generated name and no header
  fpmutil -c EOSM -m mv1080 -o focusmap.pbm     will save '.pbm' 1808x1190 graphical image map
  fpmutil input.mlv                             will save '.fpm' with the resolution taken from '.mlv'
  fpmutil -n input.mlv                          will save '.fpm' with the resolution taken from '.mlv' and no header
  fpmutil -m croprec input.mlv                  will save '.fpm' with crop_rec modes compliant pattern
  fpmutil -m croprec input.mlv -o output,pbm    will save '.pbm' files for each croprec pass
  fpmutil input.[fpm|pbm] -o output.[pbm|fpm]   will save '.pbm/fpm' converted from input to output format
  fpmutil input.[fpm|pbm]                       will save '.pbm/fpm' converted to opposite format of the input format
  fpmutil -c 100D -m croprec input.fpm          will save '.pbm' with overriden camera ID and video mode
  fpmutil input1.pbm input2.pbm                 will save '.fpm' with combined pixels from all input files as multipass map
  fpmutil -n input.pbm                          will save '.fpm' without header


```

Generates maps according to command line switches or MLV file info blocks.

Note: PBM (portable bitmap format - https://en.wikipedia.org/wiki/Netpbm_format) fully supported by many image editors (e.g. gimp, etc)
***
mlv_setframes : command line utility which automatically sets proper frameCount value to MLV file header.


```

usage:

mlv_setframes file.mlv [--set]

   --set    if specified actually writes frameCount to file
            otherwise just outputs the information

   Extra testing option:
   --set0x00000000    sets zero frameCount to any mlv file

```

The binary looks for proper MLV/MXX file not by extension but a content of a file, makes sure the file has to be changed and only after that alters the value if additionally --set option specified.

If --set is not specified it changes nothing - just outputs a few info about processed files. With --set0x00000000 you can go back to original state.

Note: It does not alter file modification time.