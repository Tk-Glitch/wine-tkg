/* libtiff/tif_config.h.  Generated from tif_config.h.in by configure.  */
/* libtiff/tif_config.h.in.  Not generated, but originated from autoheader.  */

#include "tiffconf.h"

/* Support CCITT Group 3 & 4 algorithms */
#define CCITT_SUPPORT 1

/* Pick up YCbCr subsampling info from the JPEG data stream to support files
   lacking the tag (default enabled). */
#define CHECK_JPEG_YCBCR_SUBSAMPLING 1

/* enable partial strip reading for large strips (experimental) */
/* #undef CHUNKY_STRIP_READ_SUPPORT */

/* Support C++ stream API (requires C++ compiler) */
/* #undef CXX_SUPPORT */

/* enable deferred strip/tile offset/size loading */
/* #undef DEFER_STRILE_LOAD */

/* Define to 1 if you have the <assert.h> header file. */
#define HAVE_ASSERT_H 1

/* Define to 1 if you have the declaration of `optarg', and to 0 if you don't.
   */
/* #undef HAVE_DECL_OPTARG */

/* Define to 1 if you have the <fcntl.h> header file. */
#define HAVE_FCNTL_H 1

/* Define to 1 if fseeko (and presumably ftello) exists and is declared. */
#define HAVE_FSEEKO 1

/* Define to 1 if you have the `getopt' function. */
/* #undef HAVE_GETOPT */

/* Define to 1 if you have the <GLUT/glut.h> header file. */
/* #undef HAVE_GLUT_GLUT_H */

/* Define to 1 if you have the <GL/glut.h> header file. */
/* #undef HAVE_GL_GLUT_H */

/* Define to 1 if you have the <GL/glu.h> header file. */
#define HAVE_GL_GLU_H 1

/* Define to 1 if you have the <GL/gl.h> header file. */
#define HAVE_GL_GL_H 1

/* Define to 1 if you have the <io.h> header file. */
#define HAVE_IO_H 1

/* Define to 1 if you have the `jbg_newlen' function. */
/* #undef HAVE_JBG_NEWLEN */

/* Define to 1 if you have the `mmap' function. */
/* #undef HAVE_MMAP */

/* Define to 1 if you have the <OpenGL/glu.h> header file. */
/* #undef HAVE_OPENGL_GLU_H */

/* Define to 1 if you have the <OpenGL/gl.h> header file. */
/* #undef HAVE_OPENGL_GL_H */

/* Define to 1 if you have the `setmode' function. */
#define HAVE_SETMODE 1

/* Define to 1 if you have the `snprintf' function. */
/* #undef HAVE_SNPRINTF */

/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* 8/12 bit libjpeg dual mode enabled */
/* #undef JPEG_DUAL_MODE_8_12 */

/* Support LERC compression */
/* #undef LERC_SUPPORT */

/* 12bit libjpeg primary include file with path */
/* #undef LIBJPEG_12_PATH */

/* Support LZMA2 compression */
/* #undef LZMA_SUPPORT */

/* Name of package */
#define PACKAGE "tiff"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT "tiff@lists.maptools.org"

/* Define to the full name of this package. */
#define PACKAGE_NAME "LibTIFF Software"

/* Define to the full name and version of this package. */
#define PACKAGE_STRING "LibTIFF Software 4.3.0"

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME "tiff"

/* Define to the home page for this package. */
#define PACKAGE_URL ""

/* Define to the version of this package. */
#define PACKAGE_VERSION "4.3.0"

/* The size of `size_t', as computed by sizeof. */
/* #undef SIZEOF_SIZE_T */

/* Default size of the strip in bytes (when strip chopping enabled) */
#define STRIP_SIZE_DEFAULT 8192

/* define to use win32 IO system */
#define USE_WIN32_FILEIO 1

/* Version number of package */
#define VERSION "4.3.0"

/* Support webp compression */
/* #undef WEBP_SUPPORT */

/* Define WORDS_BIGENDIAN to 1 if your processor stores words with the most
   significant byte first (like Motorola and SPARC, unlike Intel). */
#if defined AC_APPLE_UNIVERSAL_BUILD
# if defined __BIG_ENDIAN__
#  define WORDS_BIGENDIAN 1
# endif
#else
# ifndef WORDS_BIGENDIAN
/* #  undef WORDS_BIGENDIAN */
# endif
#endif

/* Support zstd compression */
/* #undef ZSTD_SUPPORT */

/* Enable large inode numbers on Mac OS X 10.5.  */
#ifndef _DARWIN_USE_64_BIT_INODE
# define _DARWIN_USE_64_BIT_INODE 1
#endif

/* Number of bits in a file offset, on hosts where this is settable. */
#define _FILE_OFFSET_BITS 64

/* Define to 1 to make fseeko visible on some hosts (e.g. glibc 2.2). */
/* #undef _LARGEFILE_SOURCE */

/* Define for large files, on AIX-style hosts. */
/* #undef _LARGE_FILES */

#if !defined(__MINGW32__)
#  define TIFF_SIZE_FORMAT "zu"
#  define TIFF_SSIZE_FORMAT "zd"
#else
#  define TIFF_SIZE_FORMAT "Iu"
#  define TIFF_SSIZE_FORMAT "Id"
#endif
