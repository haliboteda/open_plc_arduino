/*
 * openplc_app_version.h
 *
 * The sketch declares its own version; the core only declares the symbol.
 * A sketch that does not define it fails to link, which is deliberate: the
 * upload tool refuses to flash firmware older than what the board already
 * runs, and it needs a version to compare against.
 *
 * Not the same thing as OPENPLC_FW_VERSION. That one is the board package's
 * release version (boards.txt build.fw_version) and is identical for every
 * sketch built with the same package. See $PROD/GLOSSARY.md.
 */

#ifndef OPENPLC_APP_VERSION_H_
#define OPENPLC_APP_VERSION_H_

#ifdef __cplusplus
extern "C" {
#endif

/* Defined by the sketch through OPENPLC_APP_VERSION(). Deliberately not
 * defined here -- a sketch without a version must fail to link. */
extern const char openplc_app_version[];

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

/*
 * Write once near the top of the sketch:
 *
 *     OPENPLC_APP_VERSION(1, 0, 0);
 *
 * extern "C" is not decoration. In C++ a const object at namespace scope has
 * internal linkage, so without it the symbol is never exported and the core
 * cannot see it; it also stops name mangling.
 *
 * The three assertions reject anything that is not an integer constant in
 * 0..255. That is also what rejects a pre-release suffix such as "1.0.0-rc1":
 * it is not a constant expression, so it fails at the assertion.
 *
 * The named section is what lets postbuild.sh pull the string straight out of
 * the ELF with objcopy -j, so the .version file the upload tool reads is the
 * very bytes that are in the firmware. `used` keeps --gc-sections off it.
 */
#define OPENPLC_APP_VERSION(maj, min, pat)                                    \
    static_assert((maj) >= 0 && (maj) <= 255, "version field out of range");  \
    static_assert((min) >= 0 && (min) <= 255, "version field out of range");  \
    static_assert((pat) >= 0 && (pat) <= 255, "version field out of range");  \
    extern "C" const char openplc_app_version[]                               \
        __attribute__((used, section(".openplc_version")))                    \
        = #maj "." #min "." #pat

#endif /* __cplusplus */

#endif /* OPENPLC_APP_VERSION_H_ */
