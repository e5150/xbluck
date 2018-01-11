# xbluck
X11 blurring locker
```
usage: ./xbluck [-T timeout] [-L logfile] [-B borderwidth] [--hash hash] [colours] [filters]
Options:
	--timeout <msec> : Timeout after successfully unlocking
	                   default: 250
	--logfile <path> : Logfile for unlock attempts.
	                   (stderr in none, unless -q)
	                   default: 
	--hash <hash>    : Password hash to use instead of
	                   user's default. See crypt(3).
	--genhash[=salt] : Prompts for password and prints its hash.
	--border <width> : Width of border. default: 5
	-D               : Enable debugging, may be given multiple times
	                   At debug level 1, any three bytes is taken
	                   to be a valid password.
Colours: Border colour for the different runtime states.
	--colour-locked  (default: #101010)
	--colour-input   (default: #005577)
	--colour-erase   (default: #C08030)
	--colour-failed  (default: #FF2010)
	--colour-unlock  (default: #407040)
Filters: If any filter(s) is given on the command line, it
will be used instead of the filter(s) given at compile time.
Multiple filters can be chained and/or repeated in any order,
and will be applied as given on the command line.
	-g|--blur <r>           : Gaußian blur by <r> radius.
	-p|--pixelate <s>       : Pixelation of <s>x<s>
	-c|--colourise <colour> : Colourise image with <colour>=#AARRGGBB
	-n|--noise <±level>     : Add random noise of [<-level>, <+level>]
	                        : to each colour channel.
	-t|--tile <x,y>         : Tile miniature screenshots
	-i|--invert             : Invert all colours
	-S|--null               : No-op filter
	-F|--flip               : Flip image
	-f|--flop               : Flop image
	-E|--edge               : Edge-detection
	-Z|--shift <n>          : Shift every line by ±<n> pixels
	-G|--grey               : Convert to grey-scale
```

[screenshot](https://raw.githubusercontent.com/e5150/xbluck/master/screenshot.jpg)
