# Userspace fixes that are not Z2 specific

These are not device drivers. They are bugs that surface on 32-bit ARM and
will affect anyone running a current GNOME stack on armv7 hardware, phone or
not.

`gnome-software-0001` and `-0002`
: GNOME Software crashes on armv7 because flags are passed through varargs as
  the wrong width. One makes the `dedupe-flags` property a `guint64`, the
  other passes `refine-require-flags` to varargs constructors as the 32-bit
  flags type. Both belong upstream in GNOME Software.

Also worth knowing, though it is a packaging matter rather than a patch: the
postmarketOS systemd-repo armv7 build of `gnome-desktop` ships with some
double-precision constants stored as zero bytes. The visible symptom is that
GNOME Settings aborts on any wallpaper slideshow, inside
`gnome_bg_slide_show_get_current_slide`, because a division by a zeroed
constant produces infinity and then NaN. Alpine's build of the same upstream
version is correct. If you hit it, install Alpine's:

    apk add libgnome-bg-4=44.5-r0
