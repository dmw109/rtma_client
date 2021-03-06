__all__ = ('__title__', '__summary__', '__uri__', '__version_info__',
           '__version__', '__author__', '__maintainer__', '__email__',
           '__copyright__', '__license__')

__title__        = "librtma"
__summary__      = "Python binding for the rtma_c library."
__uri__          = "https://github.com/dmw109/rtma_client"
__version_info__ = type("version_info", (), dict(serial=9,
                        major=0, minor=0, micro=0, releaselevel="alpha"))
__version__      = "{0.major}.{0.minor}.{0.micro}{1}{2}".format(__version_info__,
                   dict(final="", alpha="a", beta="b", rc="rc")[__version_info__.releaselevel],
                   "" if __version_info__.releaselevel == "final" else __version_info__.serial)
__author__       = "David Weir"
__maintainer__   = "David Weir"
__email__        = "dmw109@pitt.edu"
__copyright__    = "Copyright (c) 2020, {0}".format(__author__)
__license__      = "MIT License ; {0}".format( "https://opensource.org/licenses/MIT")
