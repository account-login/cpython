/* appinit.c -- Tcl and Tk application initialization.

   The function Tcl_AppInit() below initializes various Tcl packages.
   It is called for each Tcl interpreter created by _tkinter.create().
   It needs to be compiled with -DWITH_<package> flags for each package
   that you are statically linking with.  You may have to add sections
   for packages not yet listed below.

   Note that those packages for which Tcl_StaticPackage() is called with
   a NULL first argument are known as "static loadable" packages to
   Tcl but not actually initialized.  To use these, you have to load
   it explicitly, e.g. tkapp.eval("load {} Blt").
 */

#include <assert.h>
#include <string.h>
#include <tcl.h>
#include <tk.h>

#include "tkinter.h"

#ifdef TKINTER_PROTECT_LOADTK
/* See Tkapp_TkInit in _tkinter.c for the usage of tk_load_faile */
static int tk_load_failed;
#endif

// mods
#define ZVFS_MOUNT "/zvfs"
#define ZVFS_TCL_DIR ZVFS_MOUNT "/tcl8.6"
#define ZVFS_TK_DIR ZVFS_MOUNT "/tk8.6"

static int setup_zvfs(Tcl_Interp *interp) {
    // check zip signatrue
    // FIXME: support zip file with comment
    // END_CENTRAL_DIR_SIZE = 22
    // STRING_END_ARCHIVE = b'PK\x05\x06'

    // open self
    // NOTE: // Tcl_FindExecutable was called in PyInit__tkinter
    const char *prog_name = Tcl_GetNameOfExecutable();
    assert(prog_name != NULL);
    FILE *fp = fopen(prog_name, "rb");
    if (!fp) {
        return 0;
    }

    // seek and read sig
    int rc = fseek(fp, -22, SEEK_END);
    if (rc != 0) {
        (void)fclose(fp);
        return 0;
    }
    char buf[4] = {0, 0, 0, 0};
    (void)fread(buf, 4, 1, fp);
    (void)fclose(fp);

    char sig[4] = {'P', 'K', 0x05, 0x06};
    if (memcmp(buf, sig, 4) != 0) {
        return 0;
    }
    // found zip sig

    extern int Zvfs_Init(Tcl_Interp *);
    extern int Zvfs_Mount(Tcl_Interp *, const char *, const char *);
    Zvfs_Init(interp);
    Zvfs_Mount(interp, Tcl_GetNameOfExecutable(), ZVFS_MOUNT);
    Tcl_SetVar2(interp, "env", "TCL_LIBRARY", ZVFS_TCL_DIR, TCL_GLOBAL_ONLY);
    Tcl_SetVar(interp, "tcl_library", ZVFS_TCL_DIR, TCL_GLOBAL_ONLY);
    return 1;
}

int
Tcl_AppInit(Tcl_Interp *interp)
{
    int zvfs_mounted = 0;
    const char *_tkinter_skip_tk_init;
#ifdef TKINTER_PROTECT_LOADTK
    const char *_tkinter_tk_failed;
#endif

#ifdef TK_AQUA
#ifndef MAX_PATH_LEN
#define MAX_PATH_LEN 1024
#endif
    char tclLibPath[MAX_PATH_LEN], tkLibPath[MAX_PATH_LEN];
    Tcl_Obj*            pathPtr;

    /* pre- Tcl_Init code copied from tkMacOSXAppInit.c */
    Tk_MacOSXOpenBundleResources (interp, "com.tcltk.tcllibrary",
    tclLibPath, MAX_PATH_LEN, 0);

    if (tclLibPath[0] != '\0') {
    Tcl_SetVar(interp, "tcl_library", tclLibPath, TCL_GLOBAL_ONLY);
        Tcl_SetVar(interp, "tclDefaultLibrary", tclLibPath, TCL_GLOBAL_ONLY);
        Tcl_SetVar(interp, "tcl_pkgPath", tclLibPath, TCL_GLOBAL_ONLY);
    }

    if (tclLibPath[0] != '\0') {
        Tcl_SetVar(interp, "tcl_library", tclLibPath, TCL_GLOBAL_ONLY);
        Tcl_SetVar(interp, "tclDefaultLibrary", tclLibPath, TCL_GLOBAL_ONLY);
        Tcl_SetVar(interp, "tcl_pkgPath", tclLibPath, TCL_GLOBAL_ONLY);
    }
#endif
    // mods
    zvfs_mounted = setup_zvfs(interp);
    if (Tcl_Init (interp) == TCL_ERROR)
        return TCL_ERROR;

#ifdef TK_AQUA
    /* pre- Tk_Init code copied from tkMacOSXAppInit.c */
    Tk_MacOSXOpenBundleResources (interp, "com.tcltk.tklibrary",
        tkLibPath, MAX_PATH_LEN, 1);

    if (tclLibPath[0] != '\0') {
        pathPtr = Tcl_NewStringObj(tclLibPath, -1);
    } else {
        Tcl_Obj *pathPtr = TclGetLibraryPath();
    }

    if (tkLibPath[0] != '\0') {
        Tcl_Obj *objPtr;

        Tcl_SetVar(interp, "tk_library", tkLibPath, TCL_GLOBAL_ONLY);
        objPtr = Tcl_NewStringObj(tkLibPath, -1);
        Tcl_ListObjAppendElement(NULL, pathPtr, objPtr);
    }

    TclSetLibraryPath(pathPtr);
#endif

#ifdef WITH_XXX
        /* Initialize modules that don't require Tk */
#endif

    _tkinter_skip_tk_init =     Tcl_GetVar(interp,
                    "_tkinter_skip_tk_init", TCL_GLOBAL_ONLY);
    if (_tkinter_skip_tk_init != NULL &&
                    strcmp(_tkinter_skip_tk_init, "1") == 0) {
        return TCL_OK;
    }

#ifdef TKINTER_PROTECT_LOADTK
    _tkinter_tk_failed = Tcl_GetVar(interp,
                    "_tkinter_tk_failed", TCL_GLOBAL_ONLY);

    if (tk_load_failed || (
                            _tkinter_tk_failed != NULL &&
                            strcmp(_tkinter_tk_failed, "1") == 0)) {
        Tcl_SetResult(interp, TKINTER_LOADTK_ERRMSG, TCL_STATIC);
        return TCL_ERROR;
    }
#endif

    // mods
    if (zvfs_mounted) {
        Tcl_SetVar2(interp, "env", "TK_LIBRARY", ZVFS_TK_DIR, TCL_GLOBAL_ONLY);
        Tcl_SetVar(interp, "tk_library", ZVFS_TK_DIR, TCL_GLOBAL_ONLY);
    }
    if (Tk_Init(interp) == TCL_ERROR) {
#ifdef TKINTER_PROTECT_LOADTK
        tk_load_failed = 1;
        Tcl_SetVar(interp, "_tkinter_tk_failed", "1", TCL_GLOBAL_ONLY);
#endif
        return TCL_ERROR;
    }

    Tk_MainWindow(interp);

#ifdef TK_AQUA
    TkMacOSXInitAppleEvents(interp);
    TkMacOSXInitMenus(interp);
#endif

#ifdef WITH_PIL /* 0.2b5 and later -- not yet released as of May 14 */
    {
        extern void TkImaging_Init(Tcl_Interp *);
        TkImaging_Init(interp);
        /* XXX TkImaging_Init() doesn't have the right return type */
        /*Tcl_StaticPackage(interp, "Imaging", TkImaging_Init, NULL);*/
    }
#endif

#ifdef WITH_PIL_OLD /* 0.2b4 and earlier */
    {
        extern void TkImaging_Init(void);
        /* XXX TkImaging_Init() doesn't have the right prototype */
        /*Tcl_StaticPackage(interp, "Imaging", TkImaging_Init, NULL);*/
    }
#endif

#ifdef WITH_TIX
    {
        extern int Tix_Init(Tcl_Interp *interp);
        extern int Tix_SafeInit(Tcl_Interp *interp);
        Tcl_StaticPackage(NULL, "Tix", Tix_Init, Tix_SafeInit);
    }
#endif

#ifdef WITH_BLT
    {
        extern int Blt_Init(Tcl_Interp *);
        extern int Blt_SafeInit(Tcl_Interp *);
        Tcl_StaticPackage(NULL, "Blt", Blt_Init, Blt_SafeInit);
    }
#endif

#ifdef WITH_TOGL
    {
        /* XXX I've heard rumors that this doesn't work */
        extern int Togl_Init(Tcl_Interp *);
        /* XXX Is there no Togl_SafeInit? */
        Tcl_StaticPackage(NULL, "Togl", Togl_Init, NULL);
    }
#endif

#ifdef WITH_XXX

#endif
    return TCL_OK;
}
