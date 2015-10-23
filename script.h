/*
AutoHotkey

Copyright 2003-2009 Chris Mallett (support@autohotkey.com)

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#ifndef script_h
#define script_h

#include "stdafx.h" // pre-compiled headers
#include "defines.h"
#include "SimpleHeap.h" // for overloaded new/delete operators.
#include "keyboard_mouse.h" // for modLR_type
#include "var.h" // for a script's variables.
#include "WinGroup.h" // for a script's Window Groups.
#include "Util.h" // for FileTimeToYYYYMMDD(), strlcpy()
#include "resources/resource.h"  // For tray icon.
#include "script_object.h"
#include "Debugger.h"
#include "exports.h"  // for addfile in script2.cpp
#include "os_version.h" // For the global OS_Version object

#include "Winternl.h"
EXTERN_OSVER; // For the access to the g_os version object without having to include globaldata.h
EXTERN_G;

#define MAX_THREADS_LIMIT UCHAR_MAX // Uses UCHAR_MAX (255) because some variables that store a thread count are UCHARs.
#define MAX_THREADS_DEFAULT 10 // Must not be higher than above.
#define EMERGENCY_THREADS 2 // This is the number of extra threads available after g_MaxThreadsTotal has been reached for the following to launch: hotkeys/etc. whose first line is something important like ExitApp or Pause. (see #MaxThreads documentation).
#define MAX_THREADS_EMERGENCY (g_MaxThreadsTotal + EMERGENCY_THREADS)
#define TOTAL_ADDITIONAL_THREADS (EMERGENCY_THREADS + 2) // See below.
// Must allow two beyond EMERGENCY_THREADS: One for the AutoExec/idle thread and one so that ExitApp()
// can run even when MAX_THREADS_EMERGENCY has been reached.
// Explanation: If/when AutoExec() finishes, although it no longer needs g_array[0] (not even
// AutoExecSectionTimeout() needs it because it either won't be called or it will return early),
// at least the following might still use g_array[0]:
// 1) Threadless (fast-mode) callbacks that have no controlling script thread; see RegisterCallbackCStub().
// 2) g_array[0].IsPaused indicates whether the script is in a paused state while idle.
// In addition, it probably simplifies the code not to reclaim g_array[0]; e.g. ++g and --g can be done
// unconditionally when creating new threads.

enum ExecUntilMode {NORMAL_MODE, UNTIL_RETURN, UNTIL_BLOCK_END, ONLY_ONE_LINE};

// It's done this way so that mAttribute can store a pointer or one of these constants.
// If it is storing a pointer for a given Action Type, be sure never to compare it
// for equality against these constants because by coincidence, the pointer value
// might just match one of them:
#define ATTR_NONE (void *)0  // Some places might rely on this being zero.
#define ATTR_TRUE (void *)1
typedef void *AttributeType;

typedef int FileLoopModeType;
#define FILE_LOOP_INVALID		0
#define FILE_LOOP_FILES_ONLY	1
#define FILE_LOOP_FOLDERS_ONLY	2
#define FILE_LOOP_RECURSE		4
#define FILE_LOOP_FILES_AND_FOLDERS (FILE_LOOP_FILES_ONLY | FILE_LOOP_FOLDERS_ONLY)

enum VariableTypeType {VAR_TYPE_INVALID, VAR_TYPE_NUMBER, VAR_TYPE_INTEGER, VAR_TYPE_FLOAT
	, VAR_TYPE_TIME	, VAR_TYPE_DIGIT, VAR_TYPE_XDIGIT, VAR_TYPE_ALNUM, VAR_TYPE_ALPHA
	, VAR_TYPE_UPPER, VAR_TYPE_LOWER, VAR_TYPE_SPACE
	, VAR_TYPE_OBJECT, VAR_TYPE_BYREF}; // v2

#define ATTACH_THREAD_INPUT \
	bool threads_are_attached = false;\
	DWORD target_thread = GetWindowThreadProcessId(target_window, NULL);\
	if (target_thread && target_thread != g_MainThreadID && !IsWindowHung(target_window))\
		threads_are_attached = AttachThreadInput(g_MainThreadID, target_thread, TRUE) != 0;
// BELOW IS SAME AS ABOVE except it checks do_activate and also does a SetActiveWindow():
#define ATTACH_THREAD_INPUT_AND_SETACTIVEWINDOW_IF_DO_ACTIVATE \
	bool threads_are_attached = false;\
	DWORD target_thread;\
	if (do_activate)\
	{\
		target_thread = GetWindowThreadProcessId(target_window, NULL);\
		if (target_thread && target_thread != g_MainThreadID && !IsWindowHung(target_window))\
			threads_are_attached = AttachThreadInput(g_MainThreadID, target_thread, TRUE) != 0;\
		SetActiveWindow(target_window);\
	}

#define DETACH_THREAD_INPUT \
	if (threads_are_attached)\
		AttachThreadInput(g_MainThreadID, target_thread, FALSE);

#define RESEED_RANDOM_GENERATOR \
{\
	FILETIME ft;\
	GetSystemTimeAsFileTime(&ft);\
	init_genrand(ft.dwLowDateTime);\
}

// Since WM_COMMAND IDs must be shared among all menus and controls, they are carefully conserved,
// especially since there are only 65,535 possible IDs.  In addition, they are assigned to ranges
// to minimize the need that they will need to be changed in the future (changing the ID of a main
// menu item, tray menu item, or a user-defined menu item [by way of increasing MAX_CONTROLS_PER_GUI]
// is bad because some scripts might be using PostMessage/SendMessage to automate AutoHotkey itself).
// For this reason, the following ranges are reserved:
// 0: unused (possibly special in some contexts)
// 1: IDOK
// 2: IDCANCEL
// 3 to 1002: GUI window control IDs (these IDs must be unique only within their parent, not across all GUI windows)
// 1003 to 65299: User Defined Menu IDs
// 65300 to 65399: Standard tray menu items.
// 65400 to 65534: main menu items (might be best to leave 65535 unused in case it ever has special meaning)
enum CommandIDs {CONTROL_ID_FIRST = IDCANCEL + 1
	, ID_USER_FIRST = MAX_CONTROLS_PER_GUI + 3 // The first ID available for user defined menu items. Do not change this (see above for why).
	, ID_USER_LAST = 65299  // The last. Especially do not change this due to scripts using Post/SendMessage to automate AutoHotkey.
	, ID_TRAY_FIRST, ID_TRAY_OPEN = ID_TRAY_FIRST
	, ID_TRAY_HELP, ID_TRAY_WINDOWSPY, ID_TRAY_RELOADSCRIPT
	, ID_TRAY_EDITSCRIPT, ID_TRAY_SUSPEND, ID_TRAY_PAUSE, ID_TRAY_EXIT
	, ID_TRAY_LAST = ID_TRAY_EXIT // But this value should never hit the below. There is debug code to enforce.
	, ID_MAIN_FIRST = 65400, ID_MAIN_LAST = 65534}; // These should match the range used by resource.h
#ifndef MINIDLL
#define GUI_INDEX_TO_ID(index) (index + CONTROL_ID_FIRST)
#define GUI_ID_TO_INDEX(id) (id - CONTROL_ID_FIRST) // Returns a small negative if "id" is invalid, such as 0.
#define GUI_HWND_TO_INDEX(hwnd) GUI_ID_TO_INDEX(GetDlgCtrlID(hwnd)) // Returns a small negative on failure (e.g. HWND not found).
#endif
// Notes about above:
// 1) Callers should call GuiType::FindControl() instead of GUI_HWND_TO_INDEX() if the hwnd might be a combobox's
//    edit control.
// 2) Testing shows that GetDlgCtrlID() is much faster than looping through a GUI window's control array to find
//    a matching HWND.

// Pixels to scroll when scroll bar button is pressed and WheelUp/Down
#define SCROLL_STEP 10;

#define ERR_ABORT_NO_SPACES _T("The current thread will exit.")
#define ERR_ABORT _T("  ") ERR_ABORT_NO_SPACES
#define WILL_EXIT _T("The program will exit.")
#define OLD_STILL_IN_EFFECT _T("The script was not reloaded; the old version will remain in effect.")
#define ERR_CONTINUATION_SECTION_TOO_LONG _T("Continuation section too long.")
#define ERR_UNRECOGNIZED_ACTION _T("This line does not contain a recognized action.")
#define ERR_NONEXISTENT_HOTKEY _T("Nonexistent hotkey.")
#define ERR_NONEXISTENT_VARIANT _T("Nonexistent hotkey variant (IfWin).")
#define ERR_NONEXISTENT_FUNCTION _T("Call to nonexistent function.")
#define ERR_UNRECOGNIZED_DIRECTIVE _T("Unknown directive.")
#define ERR_EXE_CORRUPTED _T("EXE corrupted")
#define ERR_INVALID_VALUE _T("Invalid value.")
#define ERR_PARAM_INVALID _T("Invalid parameter(s).")
#define ERR_PARAM1_INVALID _T("Parameter #1 invalid.")
#define ERR_PARAM2_INVALID _T("Parameter #2 invalid.")
#define ERR_PARAM3_INVALID _T("Parameter #3 invalid.")
#define ERR_PARAM4_INVALID _T("Parameter #4 invalid.")
#define ERR_PARAM5_INVALID _T("Parameter #5 invalid.")
#define ERR_PARAM6_INVALID _T("Parameter #6 invalid.")
#define ERR_PARAM7_INVALID _T("Parameter #7 invalid.")
#define ERR_PARAM8_INVALID _T("Parameter #8 invalid.")
#define ERR_PARAM1_REQUIRED _T("Parameter #1 required")
#define ERR_PARAM2_REQUIRED _T("Parameter #2 required")
#define ERR_PARAM3_REQUIRED _T("Parameter #3 required")
#define ERR_PARAM4_OMIT _T("Parameter #4 should be omitted in this case.")
#define ERR_PARAM2_MUST_BE_BLANK _T("Parameter #2 must be blank in this case.")
#define ERR_PARAM3_MUST_BE_BLANK _T("Parameter #3 must be blank in this case.")
#define ERR_PARAM4_MUST_BE_BLANK _T("Parameter #4 must be blank in this case.")
#define ERR_MISSING_OUTPUT_VAR _T("Requires at least one of its output variables.")
#define ERR_MISSING_OPEN_PAREN _T("Missing \"(\"")
#define ERR_MISSING_OPEN_BRACE _T("Missing \"{\"")
#define ERR_MISSING_CLOSE_PAREN _T("Missing \")\"")
#define ERR_MISSING_CLOSE_BRACE _T("Missing \"}\"")
#define ERR_MISSING_CLOSE_BRACKET _T("Missing \"]\"") // L31
#define ERR_UNEXPECTED_CLOSE_PAREN _T("Unexpected \")\"")
#define ERR_UNEXPECTED_CLOSE_BRACKET _T("Unexpected \"]\"")
#define ERR_UNEXPECTED_CLOSE_BRACE _T("Unexpected \"}\"")
#define ERR_UNEXPECTED_COMMA _T("Unexpected comma")
#define ERR_BAD_AUTO_CONCAT _T("Missing space or operator before this.")
#define ERR_MISSING_CLOSE_QUOTE _T("Missing close-quote") // No period after short phrases.
#define ERR_MISSING_COMMA _T("Missing comma")             //
#define ERR_MISSING_PARAM_NAME _T("Missing parameter name.")
#define ERR_PARAM_REQUIRED _T("Missing a required parameter.")
#define ERR_TOO_MANY_PARAMS _T("Too many parameters passed to function.") // L31
#define ERR_TOO_FEW_PARAMS _T("Too few parameters passed to function.") // L31
#define ERR_BAD_OPTIONAL_PARAM _T("Expected \":=\"")
#define ERR_ELSE_WITH_NO_IF _T("ELSE with no matching IF")
#define ERR_UNTIL_WITH_NO_LOOP _T("UNTIL with no matching LOOP")
#define ERR_CATCH_WITH_NO_TRY _T("CATCH with no matching TRY")
#define ERR_FINALLY_WITH_NO_PRECEDENT _T("FINALLY with no matching TRY or CATCH")
#define ERR_BAD_JUMP_INSIDE_FINALLY _T("Jumps cannot exit a FINALLY block.")
#define ERR_BAD_JUMP_OUT_OF_FUNCTION _T("Cannot jump from inside a function to outside.")
#define ERR_EXPECTED_BLOCK_OR_ACTION _T("Expected \"{\" or single-line action.")
#define ERR_OUTOFMEM _T("Out of memory.")  // Used by RegEx too, so don't change it without also changing RegEx to keep the former string.
#define ERR_EXPR_TOO_LONG _T("Expression too complex")
#define ERR_TOO_MANY_REFS ERR_EXPR_TOO_LONG // No longer applies to just var/func refs. Old message: "Too many var/func refs."
#define ERR_NO_LABEL _T("Target label does not exist.")
#define ERR_MENU _T("Menu does not exist.")
#define ERR_SUBMENU _T("Submenu does not exist.")
#define ERR_WINDOW_PARAM _T("Requires at least one of its window parameters.")
#define ERR_MENUTRAY _T("Supported only for the tray menu")
#define ERR_MOUSE_COORD _T("X & Y must be either both absent or both present.")
#define ERR_DIVIDEBYZERO _T("Divide by zero")
#define ERR_VAR_IS_READONLY _T("Not allowed as an output variable.")
#define ERR_INVALID_CHAR _T("This character is not allowed here.")
#define ERR_INVALID_DOT _T("Ambiguous or invalid use of \".\"")
#define ERR_UNQUOTED_NON_ALNUM _T("Unquoted literals may only consist of alphanumeric characters/underscore.")
#define ERR_DUPLICATE_DECLARATION _T("Duplicate declaration.")
#define ERR_INVALID_CLASS_VAR _T("Invalid class variable declaration.")
#define ERR_INVALID_LINE_IN_CLASS_DEF _T("Not a valid method, class or property definition.")
#define ERR_INVALID_LINE_IN_PROPERTY_DEF _T("Not a valid property getter/setter.")
#define ERR_INVALID_GUI_NAME _T("Invalid Gui name.")
#define ERR_INVALID_OPTION _T("Invalid option.") // Generic message used by Gui and GuiControl/Get.
#define ERR_MUST_INIT_STRUCT _T("Empty pointer, dynamic Structure fields must be initialized manually first.")
#define ERR_MUST_DECLARE _T("This variable must be declared.")
#define ERR_REMOVE_THE_PERCENT _T("If this variable was not intended to be dynamic, remove the % symbols from it.")
#define ERR_DYNAMIC_TOO_LONG _T("This dynamically built variable name is too long.  ") ERR_REMOVE_THE_PERCENT
#define ERR_DYNAMIC_BLANK _T("This dynamic variable is blank.  ") ERR_REMOVE_THE_PERCENT
#define ERR_HOTKEY_IF_EXPR _T("Parameter #2 must match an existing #If expression.")
#define ERR_EXCEPTION _T("An exception was thrown.")
#define ERR_EXPR_EVAL _T("Error evaluating expression.  There may be a syntax error.")
#define ERR_NO_OBJECT _T("No object to invoke.")
#define ERR_NO_MEMBER _T("Unknown property or method.")
#define ERR_NO_GUI _T("No default GUI.")
#define ERR_NO_STATUSBAR _T("No StatusBar.")
#define ERR_NO_LISTVIEW _T("No ListView.")
#define ERR_NO_TREEVIEW _T("No TreeView.")
#define ERR_PCRE_EXEC _T("PCRE execution error.")
#define ERR_ARRAY_NOT_MULTIDIM _T("Array is not multi-dimensional.")
#define ERR_NEW_NO_CLASS _T("Missing class object for \"new\" operator.")
#define ERR_INVALID_ARG_TYPE _T("Invalid arg type.")
#define ERR_INVALID_RETURN_TYPE _T("Invalid return type.")
#define ERR_INVALID_LENGTH _T("Invalid Length.")
#define ERR_INVALID_USAGE _T("Invalid usage.")

#define WARNING_USE_UNSET_VARIABLE _T("This variable has not been assigned a value.")
#define WARNING_LOCAL_SAME_AS_GLOBAL _T("This local variable has the same name as a global variable.")
#define WARNING_USE_ENV_VARIABLE _T("An environment variable is being accessed; see #NoEnv.")

//----------------------------------------------------------------------------------

void DoIncrementalMouseMove(int aX1, int aY1, int aX2, int aY2, int aSpeed);
DWORD ProcessExist9x2000(LPTSTR aProcess);
#ifdef CONFIG_WINNT4
DWORD ProcessExistNT4(LPTSTR aProcess);
#endif

inline DWORD ProcessExist(LPTSTR aProcess)
{
	return 
#ifdef CONFIG_WINNT4
		g_os.IsWinNT4() ? ProcessExistNT4(aProcess) :
#endif
		ProcessExist9x2000(aProcess);
}

DWORD GetProcessName(DWORD aProcessID, LPTSTR aBuf, DWORD aBufSize, bool aGetNameOnly);

bool Util_Shutdown(int nFlag);
BOOL Util_ShutdownHandler(HWND hwnd, DWORD lParam);
void Util_WinKill(HWND hWnd);

enum MainWindowModes {MAIN_MODE_NO_CHANGE, MAIN_MODE_LINES, MAIN_MODE_VARS
	, MAIN_MODE_HOTKEYS, MAIN_MODE_KEYHISTORY, MAIN_MODE_REFRESH};
ResultType ShowMainWindow(MainWindowModes aMode = MAIN_MODE_NO_CHANGE, bool aRestricted = true);
DWORD GetAHKInstallDir(LPTSTR aBuf);

#ifndef MINIDLL
struct InputBoxType
{
	LPTSTR title;
	LPTSTR text;
	int width;
	int height;
	int xpos;
	int ypos;
	Var *output_var;
	TCHAR password_char;
	bool set_password_char;
	LPTSTR default_string;
	DWORD timeout;
	HWND hwnd;
	HFONT font;
};

// From AutoIt3's InputBox.  This doesn't add a measurable amount of code size, so the compiler seems to implement
// it efficiently (somewhat like a macro).
template <class T>
inline void swap(T &v1, T &v2) {
	T tmp=v1;
	v1=v2;
	v2=tmp;
}

// The following functions are used in GUI DPI scaling, so that
// GUIs designed for a 96 DPI setting (i.e. using absolute coords
// or explicit widths/sizes) can continue to run with mostly no issues.

static inline int DPIScale(int x)
{
	extern int g_ScreenDPI;
	return MulDiv(x, g_ScreenDPI, 96);
}

static inline int DPIUnscale(int x)
{
	extern int g_ScreenDPI;
	return MulDiv(x, 96, g_ScreenDPI);
}

#define INPUTBOX_DEFAULT INT_MIN
ResultType InputBoxParseOptions(LPTSTR aOptions, InputBoxType &aInputBox);
ResultType InputBox(Var *aOutputVar, LPTSTR aTitle, LPTSTR aText, LPTSTR aOptions, LPTSTR aDefault);
INT_PTR CALLBACK InputBoxProc(HWND hWndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
VOID CALLBACK InputBoxTimeout(HWND hWnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime);
#endif
VOID CALLBACK DerefTimeout(HWND hWnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime);
BOOL CALLBACK EnumChildFindSeqNum(HWND aWnd, LPARAM lParam);
BOOL CALLBACK EnumChildFindPoint(HWND aWnd, LPARAM lParam);
BOOL CALLBACK EnumChildGetControlList(HWND aWnd, LPARAM lParam);
BOOL CALLBACK EnumMonitorProc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM lParam);
BOOL CALLBACK EnumChildGetText(HWND aWnd, LPARAM lParam);
LRESULT CALLBACK MainWindowProc(HWND hWnd, UINT iMsg, WPARAM wParam, LPARAM lParam);
bool HandleMenuItem(HWND aHwnd, WORD aMenuItemID, HWND aGuiHwnd);

typedef UINT LineNumberType;
typedef WORD FileIndexType; // Use WORD to conserve memory due to its use in the Line class (adjacency to other members and due to 4-byte struct alignment).
#define ABSOLUTE_MAX_SOURCE_FILES 0xFFFF // Keep this in sync with the capacity of the type above.  Actually it could hold 0xFFFF+1, but avoid the final item for maintainability (otherwise max-index won't be able to fit inside a variable of that type).

#define LOADING_FAILED UINT_MAX

// -2 for the beginning and ending g_DerefChars:
#define MAX_VAR_NAME_LENGTH (UCHAR_MAX - 2)
#define MAX_FUNCTION_PARAMS UCHAR_MAX // Also conserves stack space to support future attributes such as param default values.
#define MAX_DEREFS_PER_ARG 512

typedef WORD DerefLengthType; // WORD might perform better than UCHAR, but this can be changed to UCHAR if another field is ever needed in the struct.
typedef UCHAR DerefParamCountType;

// Traditionally DerefType was used to hold var and func references, which are parsed at an
// early stage, but when the capability to nest expressions between percent signs was added,
// it became necessary to pre-parse more.  All non-numeric operands are represented in it.
enum DerefTypeType : BYTE
{
	DT_VAR,			// Variable reference, including built-ins.
	DT_DOUBLE,		// Marks the end of a double-deref.
	DT_STRING,		// Segment of text in a text arg (delimited by '%').
	DT_QSTRING,		// Segment of text in a quoted string (delimited by '%').
	DT_WORDOP,		// Word operator: and, or, not, new.
	// DerefType::is_function() requires that these are last:
	DT_FUNC,		// Function call.
	DT_VARIADIC		// Variadic function call.
};

class Func; // Forward declaration for use below.
#ifndef AUTOHOTKEYSC
struct FuncLibrary
{
	LPTSTR path;
	DWORD_PTR length;
};
#endif
struct DerefType
{
	LPTSTR marker;
	union
	{
		Var *var; // DT_VAR
		Func *func; // DT_FUNC
		DerefType *next; // DT_STRING
		SymbolType symbol; // DT_WORDOP
	};
	// Keep any fields that aren't an even multiple of 4 adjacent to each other.  This conserves memory
	// due to byte-alignment:
	DerefTypeType type;
	bool is_function() { return type >= DT_FUNC; }
	DerefParamCountType param_count; // The actual number of parameters present in this function *call*.  Left uninitialized except for functions.
	DerefLengthType length; // Listed only after byte-sized fields, due to it being a WORD.
};

typedef UCHAR ArgTypeType;  // UCHAR vs. an enum, to save memory.
typedef WORD ArgLengthType; // Relies on the fact that an arg's literal text can't be longer than LINE_SIZE.
#define ARG_TYPE_NORMAL     (UCHAR)0
#define ARG_TYPE_INPUT_VAR  (UCHAR)1
#define ARG_TYPE_OUTPUT_VAR (UCHAR)2
#define ARGMAP_END_MARKER ((ArgLengthType)~0) // ExpressionToPostfix() may rely on this being greater than any possible arg character offset.

struct ArgStruct
{
	ArgTypeType type;
	bool is_expression; // Whether this ARG is known to contain an expression.
	// Above are kept adjacent to each other to conserve memory (any fields that aren't an even
	// multiple of 4, if adjacent to each other, consume less memory due to default byte alignment
	// setting [which helps performance]).
	ArgLengthType length; // Keep adjacent to above so that it uses no extra memory. This member was added in v1.0.44.14 to improve runtime performance.
	LPTSTR text;
	DerefType *deref;  // Will hold a NULL-terminated array of operands/word-operators pre-parsed by ParseDerefs()/ParseOperands().
	ExprTokenType *postfix;  // An array of tokens in postfix order. Also used for ACT_(NOT)BETWEEN to store pre-converted binary integers.
};

#define BIF_DECL_PARAMS ResultToken &aResultToken, ExprTokenType *aParam[], int aParamCount

// The following macro is used for definitions and declarations of built-in functions:
#define BIF_DECL(name) void name(BIF_DECL_PARAMS)

#define _f__oneline(act)		do { act } while (0)		// Make the macro safe to use like a function, under if(), etc.
#define _f__ret(act)			_f__oneline( act; return; )	// BIFs have no return value.
#define _o__ret(act)			return (act)				// IObject::Invoke() returns ResultType.
// The following macros are used in built-in functions and objects to reduce code repetition
// and facilitate changes to the script "ABI" (i.e. the way in which parameters and return
// values are passed around).  For instance, the built-in functions might someday be exposed
// via COM IDispatch or coupled with different scripting languages.
#define _f_return(...)			_f__ret(aResultToken.Return(__VA_ARGS__))
#define _o_return(...)			_o__ret(aResultToken.Return(__VA_ARGS__))
#define _f_throw(...)			_f__ret(aResultToken.Error(__VA_ARGS__))
#define _o_throw(...)			_o__ret(aResultToken.Error(__VA_ARGS__))
// The _f_set_retval macros should be used with care because the integer macros assume symbol
// is set to its default value; i.e. don't set a string and then attempt to return an integer.
// It is also best for maintainability to avoid setting mem_to_free or an object without
// returning if there's any chance _f_throw() will be used, since in that case the caller
// may or may not Free() the result.  _f_set_retval_i() may also invalidate _f_retval_buf.
//#define _f_set_retval(...)		aResultToken.Return(__VA_ARGS__)  // Overrides the default return value but doesn't return.
#define _f_set_retval_i(n)		(aResultToken.value_int64 = static_cast<__int64>(n)) // Assumes symbol == SYM_INTEGER, the default for BIFs.
#define _f_set_retval_p(...)	aResultToken.ReturnPtr(__VA_ARGS__) // Overrides the default return value but doesn't return.  Arg must already be in persistent memory.
#define _f_return_i(n)			_f__ret(_f_set_retval_i(n)) // Return an integer.  Reduces code size vs _f_return() by assuming symbol == SYM_INTEGER, the default for BIFs.
#define _f_return_b				_f_return_i // Boolean.  Currently just returns an int because we have no boolean type.
#define _f_return_p(...)		_f__ret(_f_set_retval_p(__VA_ARGS__)) // Return a string which is already in persistent memory.
#define _o_return_p(...)		_o__ret(_f_set_retval_p(__VA_ARGS__)) // Return a string which is already in persistent memory.
#define _f_return_retval		return  // Return the value set by _f_set_retval().
#define _f_return_empty			_f_return_p(_T(""), 0)
#define _o_return_empty			return OK  // Default return value for Invoke is "".
#define _o_return_or_throw(p)	if (p) _o_return(p); else _o_throw(ERR_OUTOFMEM);
#define _f_retval_buf			(aResultToken.buf)
#define _f_retval_buf_size		MAX_NUMBER_SIZE
#define _f_number_buf			_f_retval_buf  // An alias to show intended usage, and in case the buffer size is changed.
#define _f_callee_id			(aResultToken.func->mID)


// Some of these lengths and such are based on the MSDN example at
// http://msdn.microsoft.com/library/default.asp?url=/library/en-us/sysinfo/base/enumerating_registry_subkeys.asp:
// FIX FOR v1.0.48: 
// OLDER (v1.0.44.07): Someone reported that a stack overflow was possible, implying that it only happens
// during extremely deep nesting of subkey names (perhaps a hundred or more nested subkeys).  Upon review, it seems
// that the prior limit of 16383 for value-name-length is higher than needed; testing shows that a value name can't
// be longer than 259 (limit might even be 255 if API vs. RegEdit is used to create the name).  Testing also shows
// that the total path name of a registry item (including item/value name but excluding the name of the root key)
// obeys the same limit BUT ONLY within the RegEdit GUI.  RegEdit seems capable of importing subkeys whose names
// (even without any value name appended) are longer than 259 characters (see comments higher above).
#define MAX_REG_ITEM_SIZE 1024 // Needs to be greater than 260 (see comments above), but I couldn't find any documentation at MSDN or the web about the max length of a subkey name.  One example at MSDN RegEnumKeyEx() uses MAX_KEY_LENGTH=255 and MAX_VALUE_NAME=16383, but clearly MAX_KEY_LENGTH should be larger.
#define REG_SUBKEY -2 // Custom type, not standard in Windows.
struct RegItemStruct
{
	HKEY root_key_type, root_key;  // root_key_type is always a local HKEY, whereas root_key can be a remote handle.
	TCHAR subkey[MAX_REG_ITEM_SIZE];  // The branch of the registry where this subkey or value is located.
	TCHAR name[MAX_REG_ITEM_SIZE]; // The subkey or value name.
	DWORD type; // Value Type (e.g REG_DWORD).
	FILETIME ftLastWriteTime; // Non-initialized.
	void InitForValues() {ftLastWriteTime.dwHighDateTime = ftLastWriteTime.dwLowDateTime = 0;}
	void InitForSubkeys() {type = REG_SUBKEY;}  // To distinguish REG_DWORD and such from the subkeys themselves.
	RegItemStruct(HKEY aRootKeyType, HKEY aRootKey, LPTSTR aSubKey)
		: root_key_type(aRootKeyType), root_key(aRootKey), type(REG_NONE)
	{
		*name = '\0';
		// Make a local copy on the caller's stack so that if the current script subroutine is
		// interrupted to allow another to run, the contents of the deref buffer is saved here:
		tcslcpy(subkey, aSubKey, _countof(subkey));
		// Even though the call may work with a trailing backslash, it's best to remove it
		// so that consistent results are delivered to the user.  For example, if the script
		// is enumerating recursively into a subkey, subkeys deeper down will not include the
		// trailing backslash when they are reported.  So the user's own subkey should not
		// have one either so that when A_ScriptSubKey is referenced in the script, it will
		// always show up as the value without a trailing backslash:
		size_t length = _tcslen(subkey);
		if (length && subkey[length - 1] == '\\')
			subkey[length - 1] = '\0';
	}
};

class TextStream; // TextIO
struct LoopReadFileStruct
{
	TextStream *mReadFile, *mWriteFile;
	TCHAR mWriteFileName[MAX_PATH];
	#define READ_FILE_LINE_SIZE (64 * 1024)
	TCHAR mCurrentLine[READ_FILE_LINE_SIZE];
	LoopReadFileStruct(TextStream *aReadFile, LPTSTR aWriteFileName)
		: mReadFile(aReadFile), mWriteFile(NULL) // mWriteFile is opened by FileAppend() only upon first use.
	{
		// Use our own buffer because caller's is volatile due to possibly being in the deref buffer:
		tcslcpy(mWriteFileName, aWriteFileName, _countof(mWriteFileName));
		*mCurrentLine = '\0';
	}
};

// TextStream flags for LoadIncludedFile (script files) and file-reading loops.
// Do not lock read/write: older versions used fopen(), which is implicitly permissive.
#define DEFAULT_READ_FLAGS (TextStream::READ | TextStream::EOL_CRLF | TextStream::EOL_ORPHAN_CR | TextStream::SHARE_READ | TextStream::SHARE_WRITE)


typedef UCHAR ArgCountType;
#define MAX_ARGS 20   // Maximum number of args used by any command.


enum DllArgTypes {
	  DLL_ARG_INVALID
	, DLL_ARG_ASTR
	, DLL_ARG_INT
	, DLL_ARG_SHORT
	, DLL_ARG_CHAR
	, DLL_ARG_INT64
	, DLL_ARG_FLOAT
	, DLL_ARG_DOUBLE
	, DLL_ARG_WSTR
	, DLL_ARG_STR  = UorA(DLL_ARG_WSTR, DLL_ARG_ASTR)
	, DLL_ARG_xSTR = UorA(DLL_ARG_ASTR, DLL_ARG_WSTR) // To simplify some sections.
};  // Some sections might rely on DLL_ARG_INVALID being 0.


// Note that currently this value must fit into a sc_type variable because that is how TextToKey()
// stores it in the hotkey class.  sc_type is currently a UINT, and will always be at least a
// WORD in size, so it shouldn't be much of an issue:
#define MAX_JOYSTICKS 16  // The maximum allowed by any Windows operating system.
#define MAX_JOY_BUTTONS 32 // Also the max that Windows supports.
enum JoyControls {JOYCTRL_INVALID, JOYCTRL_XPOS, JOYCTRL_YPOS, JOYCTRL_ZPOS
, JOYCTRL_RPOS, JOYCTRL_UPOS, JOYCTRL_VPOS, JOYCTRL_POV
, JOYCTRL_NAME, JOYCTRL_BUTTONS, JOYCTRL_AXES, JOYCTRL_INFO
, JOYCTRL_1, JOYCTRL_2, JOYCTRL_3, JOYCTRL_4, JOYCTRL_5, JOYCTRL_6, JOYCTRL_7, JOYCTRL_8  // Buttons.
, JOYCTRL_9, JOYCTRL_10, JOYCTRL_11, JOYCTRL_12, JOYCTRL_13, JOYCTRL_14, JOYCTRL_15, JOYCTRL_16
, JOYCTRL_17, JOYCTRL_18, JOYCTRL_19, JOYCTRL_20, JOYCTRL_21, JOYCTRL_22, JOYCTRL_23, JOYCTRL_24
, JOYCTRL_25, JOYCTRL_26, JOYCTRL_27, JOYCTRL_28, JOYCTRL_29, JOYCTRL_30, JOYCTRL_31, JOYCTRL_32
, JOYCTRL_BUTTON_MAX = JOYCTRL_32
};
#define IS_JOYSTICK_BUTTON(joy) (joy >= JOYCTRL_1 && joy <= JOYCTRL_BUTTON_MAX)

#ifndef MINIDLL
enum MenuCommands {MENU_CMD_INVALID, MENU_CMD_SHOW, MENU_CMD_USEERRORLEVEL
	, MENU_CMD_ADD, MENU_CMD_RENAME, MENU_CMD_CHECK, MENU_CMD_UNCHECK, MENU_CMD_TOGGLECHECK
	, MENU_CMD_ENABLE, MENU_CMD_DISABLE, MENU_CMD_TOGGLEENABLE
	, MENU_CMD_STANDARD, MENU_CMD_NOSTANDARD, MENU_CMD_COLOR, MENU_CMD_DEFAULT, MENU_CMD_NODEFAULT
	, MENU_CMD_DELETE, MENU_CMD_DELETEALL, MENU_CMD_TIP, MENU_CMD_ICON, MENU_CMD_NOICON
	, MENU_CMD_CLICK, MENU_CMD_MAINWINDOW, MENU_CMD_NOMAINWINDOW
};
#endif //MINIDLL
// Each line in the enumeration below corresponds to a group of built-in functions (defined
// in g_BIF) which are implemented using a single C++ function.  These IDs are passed to the
// C++ function to tell it which function is being called.  Each group starts at ID 0 in case
// it helps the compiler to reduce code size.
enum BuiltInFunctionID {
#ifndef MINIDLL
	FID_LV_GetNext = 0, FID_LV_GetCount,
	FID_LV_Add = 0, FID_LV_Insert, FID_LV_Modify,
	FID_LV_InsertCol = 0, FID_LV_ModifyCol, FID_LV_DeleteCol,
	FID_TV_Add = 0, FID_TV_Modify, FID_TV_Delete,
	FID_TV_GetNext = 0, FID_TV_GetPrev, FID_TV_GetParent, FID_TV_GetChild, FID_TV_GetSelection, FID_TV_GetCount,
	FID_TV_Get = 0, FID_TV_GetText,
	FID_SB_SetText = 0, FID_SB_SetParts, FID_SB_SetIcon,
#endif
	FID_Trim = 0, FID_LTrim, FID_RTrim,
	FID_RegExMatch = 0, FID_RegExReplace,
	FID_GetKeyName = 0, FID_GetKeyVK = 1, FID_GetKeySC,
	FID_StrGet = 0, FID_StrPut,
	FID_FileExist = 0, FID_DirExist,
	FID_WinExist = 0, FID_WinActive,
	FID_Floor = 0, FID_Ceil,
	FID_ASin = 0, FID_ACos,
	FID_Sqrt = 0, FID_Log, FID_Ln,
	FID_ObjAddRef = 0, FID_ObjRelease,
	FID_ObjInsertAt = 0, FID_ObjDelete, FID_ObjRemoveAt, FID_ObjPush, FID_ObjPop, FID_ObjLength, FID_ObjMaxIndex, FID_ObjMinIndex, FID_ObjHasKey, FID_ObjGetCapacity, FID_ObjSetCapacity, FID_ObjGetAddress, FID_ObjClone, FID_ObjNewEnum, FID_ObjCount,
	FID_ComObjType = 0, FID_ComObjValue,
	FID_WinGetID = 0, FID_WinGetIDLast, FID_WinGetPID, FID_WinGetProcessName, FID_WinGetProcessPath, FID_WinGetCount, FID_WinGetList, FID_WinGetMinMax, FID_WinGetControls, FID_WinGetControlsHwnd, FID_WinGetTransparent, FID_WinGetTransColor, FID_WinGetStyle, FID_WinGetExStyle,
	FID_WinSetTransparent = 0, FID_WinSetTransColor, FID_WinSetAlwaysOnTop, FID_WinSetStyle, FID_WinSetExStyle, FID_WinSetEnabled, FID_WinSetRegion,
	FID_WinMoveBottom = 0, FID_WinMoveTop,
	FID_ProcessExist = 0, FID_ProcessClose, FID_ProcessWait, FID_ProcessWaitClose, 
	FID_MonitorGet = 0, FID_MonitorGetWorkArea, FID_MonitorGetCount, FID_MonitorGetPrimary, FID_MonitorGetName, 
	FID_OnExit = 0, FID_OnClipboardChange
};

#ifndef MINIDLL
#define AHK_LV_SELECT       0x0100
#define AHK_LV_DESELECT     0x0200
#define AHK_LV_FOCUS        0x0400
#define AHK_LV_DEFOCUS      0x0800
#define AHK_LV_CHECK        0x1000
#define AHK_LV_UNCHECK      0x2000
#define AHK_LV_DROPHILITE   0x4000
#define AHK_LV_UNDROPHILITE 0x8000
// Although there's no room remaining in the BYTE for LVIS_CUT (AHK_LV_CUT) [assuming it's ever needed],
// it might be possible to squeeze more info into it as follows:
// Each pair of bits can represent three values (other than zero).  But since only two values are needed
// (since an item can't be both selected an deselected simultaneously), one value in each pair is available
// for future use such as LVIS_CUT.

enum GuiCommands {GUI_CMD_INVALID, GUI_CMD_OPTIONS, GUI_CMD_ADD, GUI_CMD_MARGIN, GUI_CMD_MENU
	, GUI_CMD_SHOW, GUI_CMD_SUBMIT, GUI_CMD_CANCEL, GUI_CMD_MINIMIZE, GUI_CMD_MAXIMIZE, GUI_CMD_RESTORE
	, GUI_CMD_DESTROY, GUI_CMD_FONT, GUI_CMD_TAB, GUI_CMD_LISTVIEW, GUI_CMD_TREEVIEW, GUI_CMD_DEFAULT
	, GUI_CMD_COLOR, GUI_CMD_FLASH, GUI_CMD_NEW
};

enum GuiControlCmds {GUICONTROL_CMD_INVALID, GUICONTROL_CMD_OPTIONS, GUICONTROL_CMD_CONTENTS, GUICONTROL_CMD_TEXT
	, GUICONTROL_CMD_MOVE, GUICONTROL_CMD_MOVEDRAW, GUICONTROL_CMD_FOCUS, GUICONTROL_CMD_ENABLE, GUICONTROL_CMD_DISABLE
	, GUICONTROL_CMD_SHOW, GUICONTROL_CMD_HIDE, GUICONTROL_CMD_CHOOSE, GUICONTROL_CMD_CHOOSESTRING
	, GUICONTROL_CMD_FONT
};

enum GuiControlGetCmds {GUICONTROLGET_CMD_INVALID, GUICONTROLGET_CMD_CONTENTS, GUICONTROLGET_CMD_POS
	, GUICONTROLGET_CMD_FOCUS, GUICONTROLGET_CMD_FOCUSV, GUICONTROLGET_CMD_ENABLED, GUICONTROLGET_CMD_VISIBLE
	, GUICONTROLGET_CMD_HWND, GUICONTROLGET_CMD_NAME
};

typedef UCHAR GuiControls;
enum GuiControlTypes {GUI_CONTROL_INVALID // GUI_CONTROL_INVALID must be zero due to things like ZeroMemory() on the struct.
	, GUI_CONTROL_TEXT, GUI_CONTROL_PIC, GUI_CONTROL_GROUPBOX
	, GUI_CONTROL_BUTTON, GUI_CONTROL_CHECKBOX, GUI_CONTROL_RADIO
	, GUI_CONTROL_DROPDOWNLIST, GUI_CONTROL_COMBOBOX
	, GUI_CONTROL_LISTBOX, GUI_CONTROL_LISTVIEW, GUI_CONTROL_TREEVIEW
	, GUI_CONTROL_EDIT, GUI_CONTROL_DATETIME, GUI_CONTROL_MONTHCAL, GUI_CONTROL_HOTKEY
	, GUI_CONTROL_UPDOWN, GUI_CONTROL_SLIDER, GUI_CONTROL_PROGRESS, GUI_CONTROL_TAB, GUI_CONTROL_TAB2
	, GUI_CONTROL_ACTIVEX, GUI_CONTROL_LINK, GUI_CONTROL_CUSTOM, GUI_CONTROL_STATUSBAR, GUI_CONTROL_GUI}; // Kept last to reflect it being bottommost in switch()s (for perf), since not too often used.
#endif // MINIDLL
enum ThreadCommands {THREAD_CMD_INVALID, THREAD_CMD_PRIORITY, THREAD_CMD_INTERRUPT, THREAD_CMD_NOTIMERS};

#define PROCESS_PRIORITY_LETTERS _T("LBNAHR")
enum ProcessCmds {PROCESS_CMD_INVALID, PROCESS_CMD_EXIST, PROCESS_CMD_CLOSE
	, PROCESS_CMD_WAIT, PROCESS_CMD_WAITCLOSE};

enum ControlCmds {CONTROL_CMD_INVALID, CONTROL_CMD_CHECK, CONTROL_CMD_UNCHECK
	, CONTROL_CMD_ENABLE, CONTROL_CMD_DISABLE, CONTROL_CMD_SHOW, CONTROL_CMD_HIDE
	, CONTROL_CMD_STYLE, CONTROL_CMD_EXSTYLE
	, CONTROL_CMD_SHOWDROPDOWN, CONTROL_CMD_HIDEDROPDOWN
	, CONTROL_CMD_TABLEFT, CONTROL_CMD_TABRIGHT
	, CONTROL_CMD_ADD, CONTROL_CMD_DELETE, CONTROL_CMD_CHOOSE
	, CONTROL_CMD_CHOOSESTRING, CONTROL_CMD_EDITPASTE};

enum ControlGetCmds {CONTROLGET_CMD_INVALID, CONTROLGET_CMD_CHECKED, CONTROLGET_CMD_ENABLED
	, CONTROLGET_CMD_VISIBLE, CONTROLGET_CMD_TAB, CONTROLGET_CMD_FINDSTRING
	, CONTROLGET_CMD_CHOICE, CONTROLGET_CMD_LIST, CONTROLGET_CMD_LINECOUNT, CONTROLGET_CMD_CURRENTLINE
	, CONTROLGET_CMD_CURRENTCOL, CONTROLGET_CMD_LINE, CONTROLGET_CMD_SELECTED
	, CONTROLGET_CMD_STYLE, CONTROLGET_CMD_EXSTYLE, CONTROLGET_CMD_HWND};

enum DriveCmds {DRIVE_CMD_INVALID, DRIVE_CMD_EJECT, DRIVE_CMD_LOCK, DRIVE_CMD_UNLOCK, DRIVE_CMD_LABEL};

enum DriveGetCmds {DRIVEGET_CMD_INVALID, DRIVEGET_CMD_LIST, DRIVEGET_CMD_FILESYSTEM, DRIVEGET_CMD_LABEL
	, DRIVEGET_CMD_SERIAL, DRIVEGET_CMD_TYPE, DRIVEGET_CMD_STATUS
	, DRIVEGET_CMD_STATUSCD, DRIVEGET_CMD_CAPACITY, DRIVEGET_CMD_SPACEFREE};


class Label; // Forward declaration so that each can use the other.
class Line
{
private:
	ResultType EvaluateCondition();
	bool EvaluateLoopUntil(ResultType &aResult);
	ResultType Line::PerformLoop(ResultToken *aResultToken, bool &aContinueMainLoop, Line *&aJumpToLine, Line *aUntil
		, __int64 aIterationLimit, bool aIsInfinite);
	ResultType Line::PerformLoopFilePattern(ResultToken *aResultToken, bool &aContinueMainLoop, Line *&aJumpToLine, Line *aUntil
		, FileLoopModeType aFileLoopMode, bool aRecurseSubfolders, LPTSTR aFilePattern);
	ResultType PerformLoopReg(ResultToken *aResultToken, bool &aContinueMainLoop, Line *&aJumpToLine, Line *aUntil
		, FileLoopModeType aFileLoopMode, bool aRecurseSubfolders, HKEY aRootKeyType, HKEY aRootKey, LPTSTR aRegSubkey);
	ResultType PerformLoopParse(ResultToken *aResultToken, bool &aContinueMainLoop, Line *&aJumpToLine, Line *aUntil);
	ResultType Line::PerformLoopParseCSV(ResultToken *aResultToken, bool &aContinueMainLoop, Line *&aJumpToLine, Line *aUntil);
	ResultType PerformLoopReadFile(ResultToken *aResultToken, bool &aContinueMainLoop, Line *&aJumpToLine, Line *aUntil, TextStream *aReadFile, LPTSTR aWriteFileName);
	ResultType PerformLoopWhile(ResultToken *aResultToken, bool &aContinueMainLoop, Line *&aJumpToLine); // Lexikos: ACT_WHILE.
	ResultType PerformLoopFor(ResultToken *aResultToken, bool &aContinueMainLoop, Line *&aJumpToLine, Line *aUntil); // Lexikos: ACT_FOR.
	ResultType Perform();
	friend BIF_DECL(BIF_PerformAction);

	ResultType MouseGetPos(DWORD aOptions);
	ResultType FormatTime(LPTSTR aYYYYMMDD, LPTSTR aFormat);
	ResultType StringReplace();
	ResultType SplitPath(LPTSTR aFileSpec);
	ResultType PerformSort(LPTSTR aContents, LPTSTR aOptions);
	ResultType DriveSpace(LPTSTR aPath, bool aGetFreeSpace);
	ResultType Drive(LPTSTR aCmd, LPTSTR aValue, LPTSTR aValue2);
	ResultType DriveLock(TCHAR aDriveLetter, bool aLockIt);
	ResultType DriveGet(LPTSTR aCmd, LPTSTR aValue);
	ResultType SoundSetGet(LPTSTR aSetting, LPTSTR aComponentType, LPTSTR aControlType, LPTSTR aDevice);
	ResultType SoundSetGet2kXP(LPTSTR aSetting, DWORD aComponentType, int aComponentInstance
		, DWORD aControlType, LPTSTR aDevice);
	ResultType SoundSetGetVista(LPTSTR aSetting, DWORD aComponentType, int aComponentInstance
		, DWORD aControlType, LPTSTR aDevice);
	ResultType SoundPlay(LPTSTR aFilespec, bool aSleepUntilDone);
	ResultType Download(LPTSTR aURL, LPTSTR aFilespec);
	ResultType FileSelect(LPTSTR aOptions, LPTSTR aWorkingDir, LPTSTR aGreeting, LPTSTR aFilter);

	// Bitwise flags:
	#define FSF_ALLOW_CREATE 0x01
	#define FSF_EDITBOX      0x02
	#define FSF_NONEWDIALOG  0x04
	ResultType DirSelect(LPTSTR aRootDir, LPTSTR aOptions, LPTSTR aGreeting);

	ResultType FileGetShortcut(LPTSTR aShortcutFile);
	ResultType FileCreateShortcut(LPTSTR aTargetFile, LPTSTR aShortcutFile, LPTSTR aWorkingDir, LPTSTR aArgs
		, LPTSTR aDescription, LPTSTR aIconFile, LPTSTR aHotkey, LPTSTR aIconNumber, LPTSTR aRunState);
	ResultType FileCreateDir(LPTSTR aDirSpec);
	ResultType FileRead(LPTSTR aFilespec);
	ResultType FileAppend(LPTSTR aFilespec, LPTSTR aBuf, LoopReadFileStruct *aCurrentReadFile);
	ResultType WriteClipboardToFile(LPTSTR aFilespec, Var *aBinaryClipVar = NULL);
	ResultType FileDelete();
	ResultType FileRecycle(LPTSTR aFilePattern);
	ResultType FileRecycleEmpty(LPTSTR aDriveLetter);
	ResultType FileInstall(LPTSTR aSource, LPTSTR aDest, LPTSTR aFlag);

	ResultType FileGetAttrib(LPTSTR aFilespec);
	int FileSetAttrib(LPTSTR aAttributes, LPTSTR aFilePattern, FileLoopModeType aOperateOnFolders
		, bool aDoRecurse, bool aCalledRecursively = false);
	ResultType FileGetTime(LPTSTR aFilespec, TCHAR aWhichTime);
	int FileSetTime(LPTSTR aYYYYMMDD, LPTSTR aFilePattern, TCHAR aWhichTime
		, FileLoopModeType aOperateOnFolders, bool aDoRecurse, bool aCalledRecursively = false);
	ResultType FileGetSize(LPTSTR aFilespec, LPTSTR aGranularity);
	ResultType FileGetVersion(LPTSTR aFilespec);

	ResultType IniRead(LPTSTR aFilespec, LPTSTR aSection, LPTSTR aKey, LPTSTR aDefault);
	ResultType IniWrite(LPTSTR aValue, LPTSTR aFilespec, LPTSTR aSection, LPTSTR aKey);
	ResultType IniDelete(LPTSTR aFilespec, LPTSTR aSection, LPTSTR aKey);
	ResultType RegRead(HKEY aRootKey, LPTSTR aRegSubkey, LPTSTR aValueName);
	ResultType RegWrite(DWORD aValueType, HKEY aRootKey, LPTSTR aRegSubkey, LPTSTR aValueName, LPTSTR aValue);
	ResultType RegDelete(HKEY aRootKey, LPTSTR aRegSubkey, LPTSTR aValueName);
	static LONG RegRemoveSubkeys(HKEY hRegKey);
	ResultType SetRegView(LPTSTR aView);

	ResultType ToolTip(LPTSTR aText, LPTSTR aX, LPTSTR aY, LPTSTR aID);
#ifndef MINIDLL
	ResultType TrayTip(LPTSTR aTitle, LPTSTR aText, LPTSTR aTimeout, LPTSTR aOptions);
	ResultType Input(); // The Input command.
#endif
	#define SW_NONE -1
	ResultType PerformShowWindow(ActionTypeType aActionType, LPTSTR aTitle = _T(""), LPTSTR aText = _T("")
		, LPTSTR aExcludeTitle = _T(""), LPTSTR aExcludeText = _T(""));
	ResultType PerformWait();

	ResultType WinMove(LPTSTR aTitle, LPTSTR aText, LPTSTR aX, LPTSTR aY
		, LPTSTR aWidth = _T(""), LPTSTR aHeight = _T(""), LPTSTR aExcludeTitle = _T(""), LPTSTR aExcludeText = _T(""));
	ResultType MenuSelect(LPTSTR aTitle, LPTSTR aText, LPTSTR aMenu1, LPTSTR aMenu2
		, LPTSTR aMenu3, LPTSTR aMenu4, LPTSTR aMenu5, LPTSTR aMenu6, LPTSTR aMenu7
		, LPTSTR aExcludeTitle, LPTSTR aExcludeText);
	ResultType ControlSend(LPTSTR aControl, LPTSTR aKeysToSend, LPTSTR aTitle, LPTSTR aText
		, LPTSTR aExcludeTitle, LPTSTR aExcludeText, bool aSendRaw);
	ResultType ControlClick(vk_type aVK, int aClickCount, LPTSTR aOptions, LPTSTR aControl
		, LPTSTR aTitle, LPTSTR aText, LPTSTR aExcludeTitle, LPTSTR aExcludeText);
	ResultType ControlMove(LPTSTR aControl, LPTSTR aX, LPTSTR aY, LPTSTR aWidth, LPTSTR aHeight
		, LPTSTR aTitle, LPTSTR aText, LPTSTR aExcludeTitle, LPTSTR aExcludeText);
	ResultType ControlGetPos(LPTSTR aControl, LPTSTR aTitle, LPTSTR aText, LPTSTR aExcludeTitle, LPTSTR aExcludeText);
	ResultType ControlGetFocus(LPTSTR aTitle, LPTSTR aText, LPTSTR aExcludeTitle, LPTSTR aExcludeText);
	ResultType ControlFocus(LPTSTR aControl, LPTSTR aTitle, LPTSTR aText
		, LPTSTR aExcludeTitle, LPTSTR aExcludeText);
	ResultType ControlSetText(LPTSTR aControl, LPTSTR aNewText, LPTSTR aTitle, LPTSTR aText
		, LPTSTR aExcludeTitle, LPTSTR aExcludeText);
	ResultType ControlGetText(LPTSTR aControl, LPTSTR aTitle, LPTSTR aText
		, LPTSTR aExcludeTitle, LPTSTR aExcludeText);
	ResultType ControlGetListView(Var &aOutputVar, HWND aHwnd, LPTSTR aOptions);
	ResultType Control(LPTSTR aCmd, LPTSTR aValue, LPTSTR aControl, LPTSTR aTitle, LPTSTR aText
		, LPTSTR aExcludeTitle, LPTSTR aExcludeText);
	ResultType ControlGet(LPTSTR aCommand, LPTSTR aValue, LPTSTR aControl, LPTSTR aTitle, LPTSTR aText
		, LPTSTR aExcludeTitle, LPTSTR aExcludeText);
#ifndef MINIDLL
	ResultType GuiControl(LPTSTR aCommand, LPTSTR aControlID, LPTSTR aParam3, Var *aParam3Var);
	ResultType GuiControlGet(LPTSTR aCommand, LPTSTR aControlID, LPTSTR aParam3);
#endif
	ResultType StatusBarGetText(LPTSTR aPart, LPTSTR aTitle, LPTSTR aText
		, LPTSTR aExcludeTitle, LPTSTR aExcludeText);
	ResultType StatusBarWait(LPTSTR aTextToWaitFor, LPTSTR aSeconds, LPTSTR aPart, LPTSTR aTitle, LPTSTR aText
		, LPTSTR aInterval, LPTSTR aExcludeTitle, LPTSTR aExcludeText);
	ResultType ScriptPostSendMessage(bool aUseSend);
	ResultType WinSetTitle(LPTSTR aTitle, LPTSTR aText, LPTSTR aNewTitle
		, LPTSTR aExcludeTitle = _T(""), LPTSTR aExcludeText = _T(""));
	ResultType WinGetTitle(LPTSTR aTitle, LPTSTR aText, LPTSTR aExcludeTitle, LPTSTR aExcludeText);
	ResultType WinGetClass(LPTSTR aTitle, LPTSTR aText, LPTSTR aExcludeTitle, LPTSTR aExcludeText);
	ResultType WinGetText(LPTSTR aTitle, LPTSTR aText, LPTSTR aExcludeTitle, LPTSTR aExcludeText);
	ResultType WinGetPos(LPTSTR aTitle, LPTSTR aText, LPTSTR aExcludeTitle, LPTSTR aExcludeText);
	ResultType EnvGet(LPTSTR aEnvVarName);
#ifndef MINIDLL
	ResultType PixelSearch(int aLeft, int aTop, int aRight, int aBottom, COLORREF aColorBGR, int aVariation
		, LPTSTR aOptions, bool aIsPixelGetColor);
	ResultType ImageSearch(int aLeft, int aTop, int aRight, int aBottom, LPTSTR aImageFile);
	ResultType PixelGetColor(int aX, int aY, LPTSTR aOptions);
#endif
	static ResultType SetToggleState(vk_type aVK, ToggleValueType &ForceLock, LPTSTR aToggleText);

public:
	static ResultType Line::IncludeFiles(bool aAllowDuplicateInclude, bool aIgnoreLoadFailure, FileLoopModeType aFileLoopMode, bool aRecurseSubfolders, LPTSTR aFilePattern);
	#define SET_S_DEREF_BUF(ptr, size) Line::sDerefBuf = ptr, Line::sDerefBufSize = size

	#define NULLIFY_S_DEREF_BUF \
	{\
		SET_S_DEREF_BUF(NULL, 0);\
		if (sDerefBufSize > LARGE_DEREF_BUF_SIZE)\
			--sLargeDerefBufs;\
	}

	#define PRIVATIZE_S_DEREF_BUF \
		LPTSTR our_deref_buf = Line::sDerefBuf;\
		size_t our_deref_buf_size = Line::sDerefBufSize;\
		SET_S_DEREF_BUF(NULL, 0) // For detecting whether ExpandExpression() caused a new buffer to be created.

	#define DEPRIVATIZE_S_DEREF_BUF \
		if (our_deref_buf)\
		{\
			if (Line::sDerefBuf)\
			{\
				free(Line::sDerefBuf);\
				if (Line::sDerefBufSize > LARGE_DEREF_BUF_SIZE)\
					--Line::sLargeDerefBufs;\
			}\
			SET_S_DEREF_BUF(our_deref_buf, our_deref_buf_size);\
		}
		//else the original buffer is NULL, so keep any new sDerefBuf that might have been created (should
		// help avg-case performance).

	static LPTSTR sDerefBuf;  // Buffer to hold the values of any args that need to be dereferenced.
	static size_t sDerefBufSize;
	static int sLargeDerefBufs;

	// Static because only one line can be Expanded at a time (not to mention the fact that we
	// wouldn't want the size of each line to be expanded by this size):
	static LPTSTR sArgDeref[MAX_ARGS];
	static Var *sArgVar[MAX_ARGS];

	// Keep any fields that aren't an even multiple of 4 adjacent to each other.  This conserves memory
	// due to byte-alignment:
	ActionTypeType mActionType; // What type of line this is.
	ArgCountType mArgc; // How many arguments exist in mArg[].
	FileIndexType mFileIndex; // Which file the line came from.  0 is the first, and it's the main script file.
	LineNumberType mLineNumber;  // The line number in the file from which the script was loaded, for debugging.

	ArgStruct *mArg; // Will be used to hold a dynamic array of dynamic Args.
	AttributeType mAttribute;
	Line *mPrevLine, *mNextLine; // The prev & next lines adjacent to this one in the linked list; NULL if none.
	Line *mRelatedLine;  // e.g. the "else" that belongs to this "if"
	Line *mParentLine; // Indicates the parent (owner) of this line.

#ifdef CONFIG_DEBUGGER
	Breakpoint *mBreakpoint;
#endif

	// Probably best to always use ARG1 even if other things have supposedly verified
	// that it exists, since it's count-check should make the dereference of a NULL
	// pointer (or accessing non-existent array elements) virtually impossible.
	// Empty-string is probably more universally useful than NULL, since some
	// API calls and other functions might not appreciate receiving NULLs.  In addition,
	// always remembering to have to check for NULL makes things harder to maintain
	// and more bug-prone.  The below macros rely upon the fact that the individual
	// elements of mArg cannot be NULL (because they're explicitly set to be blank
	// when the user has omitted an arg in between two non-blank args).  Later, might
	// want to review if any of the API calls used expect a string whose contents are
	// modifiable.
	#define RAW_ARG1 (mArgc > 0 ? mArg[0].text : _T(""))
	#define RAW_ARG2 (mArgc > 1 ? mArg[1].text : _T(""))
	#define RAW_ARG3 (mArgc > 2 ? mArg[2].text : _T(""))
	#define RAW_ARG4 (mArgc > 3 ? mArg[3].text : _T(""))
	#define RAW_ARG5 (mArgc > 4 ? mArg[4].text : _T(""))
	#define RAW_ARG6 (mArgc > 5 ? mArg[5].text : _T(""))
	#define RAW_ARG7 (mArgc > 6 ? mArg[6].text : _T(""))
	#define RAW_ARG8 (mArgc > 7 ? mArg[7].text : _T(""))

	#define LINE_RAW_ARG1 (line->mArgc > 0 ? line->mArg[0].text : _T(""))
	#define LINE_RAW_ARG2 (line->mArgc > 1 ? line->mArg[1].text : _T(""))
	#define LINE_RAW_ARG3 (line->mArgc > 2 ? line->mArg[2].text : _T(""))
	#define LINE_RAW_ARG4 (line->mArgc > 3 ? line->mArg[3].text : _T(""))
	#define LINE_RAW_ARG5 (line->mArgc > 4 ? line->mArg[4].text : _T(""))
	#define LINE_RAW_ARG6 (line->mArgc > 5 ? line->mArg[5].text : _T(""))
	#define LINE_RAW_ARG7 (line->mArgc > 6 ? line->mArg[6].text : _T(""))
	#define LINE_RAW_ARG8 (line->mArgc > 7 ? line->mArg[7].text : _T(""))
	#define LINE_RAW_ARG9 (line->mArgc > 8 ? line->mArg[8].text : _T(""))
	
	#define NEW_RAW_ARG1 (aArgc > 0 ? new_arg[0].text : _T("")) // Helps performance to use this vs. LINE_RAW_ARG where possible.
	#define NEW_RAW_ARG2 (aArgc > 1 ? new_arg[1].text : _T(""))
	#define NEW_RAW_ARG3 (aArgc > 2 ? new_arg[2].text : _T(""))
	#define NEW_RAW_ARG4 (aArgc > 3 ? new_arg[3].text : _T(""))
	#define NEW_RAW_ARG5 (aArgc > 4 ? new_arg[4].text : _T(""))
	#define NEW_RAW_ARG6 (aArgc > 5 ? new_arg[5].text : _T(""))
	#define NEW_RAW_ARG7 (aArgc > 6 ? new_arg[6].text : _T(""))
	#define NEW_RAW_ARG8 (aArgc > 7 ? new_arg[7].text : _T(""))
	#define NEW_RAW_ARG9 (aArgc > 8 ? new_arg[8].text : _T(""))
	
	#define SAVED_ARG1 (mArgc > 0 ? arg[0] : _T(""))
	#define SAVED_ARG2 (mArgc > 1 ? arg[1] : _T(""))
	#define SAVED_ARG3 (mArgc > 2 ? arg[2] : _T(""))
	#define SAVED_ARG4 (mArgc > 3 ? arg[3] : _T(""))
	#define SAVED_ARG5 (mArgc > 4 ? arg[4] : _T(""))

	// For the below, it is the caller's responsibility to ensure that mArgc is
	// large enough (either via load-time validation or a runtime check of mArgc).
	// This is because for performance reasons, the sArgVar entry for omitted args isn't
	// initialized, so may have an old/obsolete value from some previous command.
	#define OUTPUT_VAR (*sArgVar) // ExpandArgs() has ensured this first ArgVar is always initialized, so there's never any need to check mArgc > 0.
	#define ARGVARRAW1 (*sArgVar)   // i.e. sArgVar[0], and same as OUTPUT_VAR (it's a duplicate to help readability).
	#define ARGVARRAW2 (sArgVar[1]) // It's called RAW because its user is responsible for ensuring the arg
	#define ARGVARRAW3 (sArgVar[2]) // exists by checking mArgc at loadtime or runtime.
	#define ARGVAR1 ARGVARRAW1 // This first one doesn't need the check below because ExpandArgs() has ensured it's initialized.
	#define ARGVAR2 (mArgc > 1 ? sArgVar[1] : NULL) // Caller relies on the check of mArgc because for performance,
	#define ARGVAR3 (mArgc > 2 ? sArgVar[2] : NULL) // sArgVar[] isn't initialized for parameters the script
	#define ARGVAR4 (mArgc > 3 ? sArgVar[3] : NULL) // omitted entirely from the end of the parameter list.
	#define ARGVAR5 (mArgc > 4 ? sArgVar[4] : NULL)
	#define ARGVAR6 (mArgc > 5 ? sArgVar[5] : NULL)
	#define ARGVAR7 (mArgc > 6 ? sArgVar[6] : NULL)
	#define ARGVAR8 (mArgc > 7 ? sArgVar[7] : NULL)

	#define ARG1 sArgDeref[0] // These are the expanded/resolved parameters for the currently-executing command.
	#define ARG2 sArgDeref[1] // They're populated by ExpandArgs().
	#define ARG3 sArgDeref[2]
	#define ARG4 sArgDeref[3]
	#define ARG5 sArgDeref[4]
	#define ARG6 sArgDeref[5]
	#define ARG7 sArgDeref[6]
	#define ARG8 sArgDeref[7]
	#define ARG9 sArgDeref[8]
	#define ARG10 sArgDeref[9]
	#define ARG11 sArgDeref[10]

	#define TWO_ARGS    ARG1, ARG2
	#define THREE_ARGS  ARG1, ARG2, ARG3
	#define FOUR_ARGS   ARG1, ARG2, ARG3, ARG4
	#define FIVE_ARGS   ARG1, ARG2, ARG3, ARG4, ARG5
	#define SIX_ARGS    ARG1, ARG2, ARG3, ARG4, ARG5, ARG6
	#define SEVEN_ARGS  ARG1, ARG2, ARG3, ARG4, ARG5, ARG6, ARG7
	#define EIGHT_ARGS  ARG1, ARG2, ARG3, ARG4, ARG5, ARG6, ARG7, ARG8
	#define NINE_ARGS   ARG1, ARG2, ARG3, ARG4, ARG5, ARG6, ARG7, ARG8, ARG9
	#define TEN_ARGS    ARG1, ARG2, ARG3, ARG4, ARG5, ARG6, ARG7, ARG8, ARG9, ARG10
	#define ELEVEN_ARGS ARG1, ARG2, ARG3, ARG4, ARG5, ARG6, ARG7, ARG8, ARG9, ARG10, ARG11

	// If the arg's text is non-blank, it means the variable is a dynamic name such as array%i%
	// that must be resolved at runtime rather than load-time.  Therefore, this macro must not
	// be used without first having checked that arg.text is blank:
	#define VAR(arg) ((Var *)arg.deref)
	// Uses arg number (i.e. the first arg is 1, not 0).  Caller must ensure that ArgNum >= 1 and that
	// the arg in question is an input or output variable (since that isn't checked there).
	//#define ARG_HAS_VAR(ArgNum) (mArgc >= ArgNum && (*mArg[ArgNum-1].text || mArg[ArgNum-1].deref))

	// Shouldn't go much higher than 400 since the main window's Edit control is currently limited
	// to 64KB to be compatible with the Win9x limit.  Avg. line length is probably under 100 for
	// the vast majority of scripts, so 400 seems unlikely to exceed the buffer size.  Even in the
	// worst case where the buffer size is exceeded, the text is simply truncated, so it's not too bad:
	#define LINE_LOG_SIZE 400  // See above.
	static Line *sLog[LINE_LOG_SIZE];
	static DWORD sLogTick[LINE_LOG_SIZE];
	static int sLogNext;

#ifdef AUTOHOTKEYSC  // Reduces code size to omit things that are unused, and helps catch bugs at compile-time.
	static LPTSTR sSourceFile[1]; // Only need to be able to hold the main script since compiled scripts don't support dynamic including.
#else
	static LPTSTR *sSourceFile;   // Will hold an array of strings.
	static int sMaxSourceFiles;  // Maximum number of items it can currently hold.
#endif
	static int sSourceFileCount; // Number of items in the above array.

	static void FreeDerefBufIfLarge();

	ResultType ExecUntil(ExecUntilMode aMode, ResultToken *aResultToken = NULL, Line **apJumpToLine = NULL);

	// The following are characters that can't legally occur after an AND or OR.  It excludes all unary operators
	// "!~*&-+" as well as the parentheses chars "()":
	#define EXPR_CORE _T("<>=/|^,:")
	// The characters common to both EXPR_TELLTALES and EXPR_OPERAND_TERMINATORS:
	#define EXPR_COMMON _T(" \t") EXPR_CORE _T("*&~!()[]{}")  // Space and Tab are included at the beginning for performance.  L31: Added [] for array-like syntax.
	#define CONTINUATION_LINE_SYMBOLS EXPR_CORE _T(".+-*&!?~") // v1.0.46.
	// Characters whose presence in a mandatory-numeric param make it an expression for certain.
	// + and - are not included here because legacy numeric parameters can contain unary plus or minus,
	// e.g. WinMove, -%x%, -%y%:
	#define EXPR_TELLTALES EXPR_COMMON _T("\"")
	// Characters that mark the end of an operand inside an expression.  Double-quote must not be included:
	#define EXPR_OPERAND_TERMINATORS_EX_DOT EXPR_COMMON _T("%+-?\n") // L31: Used in a few places where '.' needs special treatment.
	#define EXPR_OPERAND_TERMINATORS EXPR_OPERAND_TERMINATORS_EX_DOT _T(".") // L31: Used in expressions where '.' is always an operator.
	#define EXPR_ALL_SYMBOLS EXPR_OPERAND_TERMINATORS _T("\"'")
	#define EXPR_ILLEGAL_CHARS _T("\\;`@#$") // Characters illegal in an expression.
	// The following HOTSTRING option recognizer is kept somewhat forgiving/non-specific for backward compatibility
	// (e.g. scripts may have some invalid hotstring options, which are simply ignored).  This definition is here
	// because it's related to continuation line symbols. Also, avoid ever adding "&" to hotstring options because
	// it might introduce ambiguity in the differentiation of things like:
	//    : & x::hotkey action
	//    : & *::abbrev with leading colon::
	#define IS_HOTSTRING_OPTION(chr) (cisalnum(chr) || _tcschr(_T("?*- \t"), chr))

	#define ArgLength(aArgNum) ArgIndexLength((aArgNum)-1)
	#define ArgToDouble(aArgNum) ArgIndexToDouble((aArgNum)-1)
	#define ArgToInt64(aArgNum) ArgIndexToInt64((aArgNum)-1)
	#define ArgToInt(aArgNum) (int)ArgToInt64(aArgNum) // Benchmarks show that having a "real" ArgToInt() that calls ATOI vs. ATOI64 (and ToInt() vs. ToInt64()) doesn't measurably improve performance.
	#define ArgToUInt(aArgNum) (UINT)ArgToInt64(aArgNum) // Similar to what ATOU() does.
	__int64 ArgIndexToInt64(int aArgIndex);
	double ArgIndexToDouble(int aArgIndex);
	size_t ArgIndexLength(int aArgIndex);

	ResultType ExpandArgs(ResultToken *aResultTokens = NULL);
	VarSizeType GetExpandedArgSize(Var *aArgVar[]);
	LPTSTR ExpandExpression(int aArgIndex, ResultType &aResult, ResultToken *aResultToken
		, LPTSTR &aTarget, LPTSTR &aDerefBuf, size_t &aDerefBufSize, LPTSTR aArgDeref[], size_t aExtraSize
		, Var **aArgVar = NULL);
	ResultType ExpressionToPostfix(ArgStruct &aArg);
	ResultType EvaluateHotCriterionExpression(LPTSTR aHotkeyName); // L4: Called by MainWindowProc to handle an AHK_HOT_IF_EXPR message.

	ResultType ValueIsType(ExprTokenType &aResultToken, ExprTokenType &aValue, LPTSTR aValueStr, ExprTokenType &aType, LPTSTR aTypeStr);

	ResultType Deref(Var *aOutputVar, LPTSTR aBuf);

	static bool FileIsFilteredOut(WIN32_FIND_DATA &aCurrentFile, FileLoopModeType aFileLoopMode
		, LPTSTR aFilePath, size_t aFilePathLength);

	Label *GetJumpTarget(bool aIsDereferenced);
	Label *GetJumpTarget(bool aIsDereferenced, Func *aFunc);
	Label *IsJumpValid(Label &aTargetLabel, bool aSilent = false);
	BOOL IsOutsideAnyFunctionBody();
	BOOL CheckValidFinallyJump(Line* jumpTarget);

	static HWND DetermineTargetWindow(LPTSTR aTitle, LPTSTR aText, LPTSTR aExcludeTitle, LPTSTR aExcludeText);

	
	// This is in the .h file so that it's more likely the compiler's cost/benefit estimate will
	// make it inline (since it is called from only one place).  Inline would be good since it
	// is called frequently during script loading and is difficult to macro-ize in a way that
	// retains readability.
	static ArgTypeType ArgIsVar(ActionTypeType aActionType, int aArgIndex)
	{
		switch(aArgIndex)
		{
		case 0:  // Arg #1
			switch(aActionType)
			{
			case ACT_ASSIGNEXPR:
			case ACT_DEREF:
			case ACT_STRINGLOWER:
			case ACT_STRINGUPPER:
			case ACT_STRINGREPLACE:
			case ACT_CONTROLGETFOCUS:
			case ACT_CONTROLGETTEXT:
			case ACT_CONTROLGET:
			case ACT_GUICONTROLGET:
			case ACT_STATUSBARGETTEXT:
#ifndef MINIDLL
			case ACT_INPUTBOX:
#endif			
			case ACT_RANDOM:
			case ACT_INIREAD:
			case ACT_REGREAD:
			case ACT_DRIVEGET:
			case ACT_SOUNDGET:
			case ACT_FILEREAD:
			case ACT_FILEGETATTRIB:
			case ACT_FILEGETTIME:
			case ACT_FILEGETSIZE:
			case ACT_FILEGETVERSION:
			case ACT_FILESELECT:
			case ACT_DIRSELECT:
			case ACT_MOUSEGETPOS:
			case ACT_WINGETTITLE:
			case ACT_WINGETCLASS:
			case ACT_WINGETTEXT:
			case ACT_WINGETPOS:
			case ACT_SYSGET:
			case ACT_ENVGET:
			case ACT_CONTROLGETPOS:
#ifndef MINIDLL
			case ACT_PIXELGETCOLOR:
			case ACT_PIXELSEARCH:
			case ACT_IMAGESEARCH:
			case ACT_INPUT:
#endif
			case ACT_FORMATTIME:
			case ACT_FOR:
			case ACT_CATCH:
			case ACT_SORT:
				return ARG_TYPE_OUTPUT_VAR;
			}
			break;

		case 1:  // Arg #2
			switch(aActionType)
			{
			case ACT_MOUSEGETPOS:
			case ACT_WINGETPOS:
			case ACT_CONTROLGETPOS:
#ifndef MINIDLL
			case ACT_PIXELSEARCH:
			case ACT_IMAGESEARCH:
#endif
			case ACT_SPLITPATH:
			case ACT_FILEGETSHORTCUT:
			case ACT_FOR:
				return ARG_TYPE_OUTPUT_VAR;
			}
			break;

		case 2:  // Arg #3
			switch(aActionType)
			{
			case ACT_WINGETPOS:
			case ACT_CONTROLGETPOS:
			case ACT_MOUSEGETPOS:
			case ACT_SPLITPATH:
			case ACT_FILEGETSHORTCUT:
				return ARG_TYPE_OUTPUT_VAR;
			}
			break;

		case 3:  // Arg #4
			switch(aActionType)
			{
			case ACT_WINGETPOS:
			case ACT_CONTROLGETPOS:
			case ACT_MOUSEGETPOS:
			case ACT_SPLITPATH:
			case ACT_FILEGETSHORTCUT:
			case ACT_RUN:
			case ACT_RUNWAIT:
				return ARG_TYPE_OUTPUT_VAR;
			}
			break;

		case 4:  // Arg #5
			if (aActionType == ACT_STRINGREPLACE)
				return ARG_TYPE_OUTPUT_VAR;
			// Otherwise, fall through to below:
		case 5:  // Arg #6
			if (aActionType == ACT_SPLITPATH || aActionType == ACT_FILEGETSHORTCUT)
				return ARG_TYPE_OUTPUT_VAR;
			break;

		case 6:  // Arg #7
		case 7:  // Arg #8
			if (aActionType == ACT_FILEGETSHORTCUT)
				return ARG_TYPE_OUTPUT_VAR;
		}
		// Otherwise:
		return ARG_TYPE_NORMAL;
	}



	ResultType ArgMustBeDereferenced(Var *aVar, int aArgIndex, Var *aArgVar[]);

	#define ArgHasDeref(aArgNum) ArgIndexHasDeref((aArgNum)-1)
	bool ArgIndexHasDeref(int aArgIndex)
	// This function should always be called in lieu of doing something like "strchr(arg.text, g_DerefChar)"
	// because that method is unreliable due to the possible presence of literal (escaped) g_DerefChars
	// in the text.
	// Caller must ensure that aArgIndex is 0 or greater.
	{
#ifdef _DEBUG
		if (aArgIndex < 0)
		{
			LineError(_T("DEBUG: BAD"), WARN);
			aArgIndex = 0;  // But let it continue.
		}
#endif
		if (aArgIndex >= mArgc) // Arg doesn't exist.
			return false;
		ArgStruct &arg = mArg[aArgIndex]; // For performance.
		// Return false if it's not of a type caller wants deemed to have derefs.
		if (arg.type == ARG_TYPE_NORMAL)
			return arg.deref && arg.deref[0].marker // Relies on short-circuit boolean evaluation order to prevent NULL-deref.
				|| arg.is_expression; // Return true for this case since most callers assume the arg is a simple literal string if we return false.
		else // Callers rely on input variables being seen as "true" because sometimes single isolated derefs are converted into ARG_TYPE_INPUT_VAR at load-time.
			return (arg.type == ARG_TYPE_INPUT_VAR);
	}

	static HKEY RegConvertKey(LPTSTR aBuf, LPTSTR *aSubkey = NULL, bool *aIsRemoteRegistry = NULL)
	{
		const size_t COMPUTER_NAME_BUF_SIZE = 128;

		LPTSTR key_name_pos = aBuf, computer_name_end = NULL;

		if (*aBuf == '\\' && aBuf[1] == '\\') // Something like \\ComputerName\HKLM.
		{
			if (  !(computer_name_end = _tcschr(aBuf + 2, '\\'))
				|| (computer_name_end - aBuf) >= COMPUTER_NAME_BUF_SIZE  )
				return NULL;
			key_name_pos = computer_name_end + 1;
		}

		// Copy root key name into temporary buffer for use by _tcsicmp().
		TCHAR key_name[20];
		int i;
		for (i = 0; key_name_pos[i] && key_name_pos[i] != '\\'; ++i)
		{
			if (i == 19)
				return NULL; // Too long to be valid.
			key_name[i] = key_name_pos[i];
		}
		key_name[i] = '\0';
		
		// Set output parameters for caller.
		if (aSubkey)
			*aSubkey = key_name_pos + i + (key_name_pos[i] == '\\');
		if (aIsRemoteRegistry)
			*aIsRemoteRegistry = (computer_name_end != NULL);

		HKEY root_key;
		if (!_tcsicmp(key_name, _T("HKLM")) || !_tcsicmp(key_name, _T("HKEY_LOCAL_MACHINE")))       root_key = HKEY_LOCAL_MACHINE;
		else if (!_tcsicmp(key_name, _T("HKCR")) || !_tcsicmp(key_name, _T("HKEY_CLASSES_ROOT")))   root_key = HKEY_CLASSES_ROOT;
		else if (!_tcsicmp(key_name, _T("HKCC")) || !_tcsicmp(key_name, _T("HKEY_CURRENT_CONFIG"))) root_key = HKEY_CURRENT_CONFIG;
		else if (!_tcsicmp(key_name, _T("HKCU")) || !_tcsicmp(key_name, _T("HKEY_CURRENT_USER")))   root_key = HKEY_CURRENT_USER;
		else if (!_tcsicmp(key_name, _T("HKU")) || !_tcsicmp(key_name, _T("HKEY_USERS")))           root_key = HKEY_USERS;
		else // Invalid or unsupported root key name.
			return NULL;

		if (!aIsRemoteRegistry || !computer_name_end) // Either caller didn't want it opened, or it doesn't need to be.
			return root_key; // If it's a remote key, this value should only be used by the caller as an indicator.
		// Otherwise, it's a remote computer whose registry the caller wants us to open:
		// It seems best to require the two leading backslashes in case the computer name contains
		// spaces (just in case spaces are allowed on some OSes or perhaps for Unix interoperability, etc.).
		// Therefore, make no attempt to trim leading and trailing spaces from the computer name:
		TCHAR computer_name[COMPUTER_NAME_BUF_SIZE];
		tcslcpy(computer_name, aBuf, _countof(computer_name));
		computer_name[computer_name_end - aBuf] = '\0';
		HKEY remote_key;
		return (RegConnectRegistry(computer_name, root_key, &remote_key) == ERROR_SUCCESS) ? remote_key : NULL;
	}

	static LPTSTR RegConvertRootKey(LPTSTR aBuf, size_t aBufSize, HKEY aRootKey)
	{
		// switch() doesn't directly support expression of type HKEY:
		if (aRootKey == HKEY_LOCAL_MACHINE)       tcslcpy(aBuf, _T("HKEY_LOCAL_MACHINE"), aBufSize);
		else if (aRootKey == HKEY_CLASSES_ROOT)   tcslcpy(aBuf, _T("HKEY_CLASSES_ROOT"), aBufSize);
		else if (aRootKey == HKEY_CURRENT_CONFIG) tcslcpy(aBuf, _T("HKEY_CURRENT_CONFIG"), aBufSize);
		else if (aRootKey == HKEY_CURRENT_USER)   tcslcpy(aBuf, _T("HKEY_CURRENT_USER"), aBufSize);
		else if (aRootKey == HKEY_USERS)          tcslcpy(aBuf, _T("HKEY_USERS"), aBufSize);
		else if (aBufSize)                        *aBuf = '\0'; // Make it be the empty string for anything else.
		// These are either unused or so rarely used (DYN_DATA on Win9x) that they aren't supported:
		// HKEY_PERFORMANCE_DATA, HKEY_PERFORMANCE_TEXT, HKEY_PERFORMANCE_NLSTEXT, HKEY_DYN_DATA
		return aBuf;
	}
	static int RegConvertValueType(LPTSTR aValueType)
	{
		if (!_tcsicmp(aValueType, _T("REG_SZ"))) return REG_SZ;
		if (!_tcsicmp(aValueType, _T("REG_EXPAND_SZ"))) return REG_EXPAND_SZ;
		if (!_tcsicmp(aValueType, _T("REG_MULTI_SZ"))) return REG_MULTI_SZ;
		if (!_tcsicmp(aValueType, _T("REG_DWORD"))) return REG_DWORD;
		if (!_tcsicmp(aValueType, _T("REG_BINARY"))) return REG_BINARY;
		return REG_NONE; // Unknown or unsupported type.
	}
	static LPTSTR RegConvertValueType(LPTSTR aBuf, size_t aBufSize, DWORD aValueType)
	{
		switch(aValueType)
		{
		case REG_SZ: tcslcpy(aBuf, _T("REG_SZ"), aBufSize); return aBuf;
		case REG_EXPAND_SZ: tcslcpy(aBuf, _T("REG_EXPAND_SZ"), aBufSize); return aBuf;
		case REG_BINARY: tcslcpy(aBuf, _T("REG_BINARY"), aBufSize); return aBuf;
		case REG_DWORD: tcslcpy(aBuf, _T("REG_DWORD"), aBufSize); return aBuf;
		case REG_DWORD_BIG_ENDIAN: tcslcpy(aBuf, _T("REG_DWORD_BIG_ENDIAN"), aBufSize); return aBuf;
		case REG_LINK: tcslcpy(aBuf, _T("REG_LINK"), aBufSize); return aBuf;
		case REG_MULTI_SZ: tcslcpy(aBuf, _T("REG_MULTI_SZ"), aBufSize); return aBuf;
		case REG_RESOURCE_LIST: tcslcpy(aBuf, _T("REG_RESOURCE_LIST"), aBufSize); return aBuf;
		case REG_FULL_RESOURCE_DESCRIPTOR: tcslcpy(aBuf, _T("REG_FULL_RESOURCE_DESCRIPTOR"), aBufSize); return aBuf;
		case REG_RESOURCE_REQUIREMENTS_LIST: tcslcpy(aBuf, _T("REG_RESOURCE_REQUIREMENTS_LIST"), aBufSize); return aBuf;
		case REG_QWORD: tcslcpy(aBuf, _T("REG_QWORD"), aBufSize); return aBuf;
		case REG_SUBKEY: tcslcpy(aBuf, _T("KEY"), aBufSize); return aBuf;  // Custom (non-standard) type.
		default: if (aBufSize) *aBuf = '\0'; return aBuf;  // Make it be the empty string for REG_NONE and anything else.
		}
	}
	static DWORD RegConvertView(LPTSTR aBuf)
	{
		if (!_tcsicmp(aBuf, _T("Default")))
			return 0;
		else if (!_tcscmp(aBuf, _T("32")))
			return KEY_WOW64_32KEY;
		else if (!_tcscmp(aBuf, _T("64")))
			return KEY_WOW64_64KEY;
		else
			return -1;
	}

	static DWORD SoundConvertComponentType(LPTSTR aBuf, int *aInstanceNumber = NULL)
	{
		LPTSTR colon_pos = _tcschr(aBuf, ':');
		size_t length_to_check = colon_pos ? colon_pos - aBuf : _tcslen(aBuf);
		if (aInstanceNumber) // Caller wanted the below put into the output parameter.
		{
			if (colon_pos)
			{
				*aInstanceNumber = ATOI(colon_pos + 1);
				if (*aInstanceNumber < 1)
					*aInstanceNumber = 1;
			}
			else
				*aInstanceNumber = 1;
		}
		if (!tcslicmp(aBuf, _T("Master"), length_to_check)
			|| !tcslicmp(aBuf, _T("Speakers"), length_to_check))   return MIXERLINE_COMPONENTTYPE_DST_SPEAKERS;
		if (!tcslicmp(aBuf, _T("Headphones"), length_to_check))    return MIXERLINE_COMPONENTTYPE_DST_HEADPHONES;
		if (!tcslicmp(aBuf, _T("Digital"), length_to_check))       return MIXERLINE_COMPONENTTYPE_SRC_DIGITAL;
		if (!tcslicmp(aBuf, _T("Line"), length_to_check))          return MIXERLINE_COMPONENTTYPE_SRC_LINE;
		if (!tcslicmp(aBuf, _T("Microphone"), length_to_check))    return MIXERLINE_COMPONENTTYPE_SRC_MICROPHONE;
		if (!tcslicmp(aBuf, _T("Synth"), length_to_check))         return MIXERLINE_COMPONENTTYPE_SRC_SYNTHESIZER;
		if (!tcslicmp(aBuf, _T("CD"), length_to_check))            return MIXERLINE_COMPONENTTYPE_SRC_COMPACTDISC;
		if (!tcslicmp(aBuf, _T("Telephone"), length_to_check))     return MIXERLINE_COMPONENTTYPE_SRC_TELEPHONE;
		if (!tcslicmp(aBuf, _T("PCSpeaker"), length_to_check))     return MIXERLINE_COMPONENTTYPE_SRC_PCSPEAKER;
		if (!tcslicmp(aBuf, _T("Wave"), length_to_check))          return MIXERLINE_COMPONENTTYPE_SRC_WAVEOUT;
		if (!tcslicmp(aBuf, _T("Aux"), length_to_check))           return MIXERLINE_COMPONENTTYPE_SRC_AUXILIARY;
		if (!tcslicmp(aBuf, _T("Analog"), length_to_check))        return MIXERLINE_COMPONENTTYPE_SRC_ANALOG;
		// v1.0.37.06: The following was added because it's legitimate on some sound cards such as
		// SB Audigy's recording (dest #2) Wave/Mp3 volume:
		if (!tcslicmp(aBuf, _T("N/A"), length_to_check))           return MIXERLINE_COMPONENTTYPE_SRC_UNDEFINED; // 0x1000
		return MIXERLINE_COMPONENTTYPE_DST_UNDEFINED; // Zero.
	}
	static DWORD SoundConvertControlType(LPTSTR aBuf)
	{
		// v1.0.37.06: The following was added to allow unnamed control types (if any) to be accessed via number:
		if (IsNumeric(aBuf, false, false, true)) // Seems best to allowing floating point here, since .000 on the end might happen sometimes.
			return ATOU(aBuf);
		// The following are the types that seem to correspond to actual sound attributes.  Some of the
		// values are not included here, such as MIXERCONTROL_CONTROLTYPE_FADER, which seems to be a type
		// of sound control rather than a quality of the sound itself.  For performance, put the most
		// often used ones up top.
		if (!_tcsicmp(aBuf, _T("Vol"))
			|| !_tcsicmp(aBuf, _T("Volume"))) return MIXERCONTROL_CONTROLTYPE_VOLUME;
		if (!_tcsicmp(aBuf, _T("OnOff")))     return MIXERCONTROL_CONTROLTYPE_ONOFF;
		if (!_tcsicmp(aBuf, _T("Mute")))      return MIXERCONTROL_CONTROLTYPE_MUTE;
		if (!_tcsicmp(aBuf, _T("Mono")))      return MIXERCONTROL_CONTROLTYPE_MONO;
		if (!_tcsicmp(aBuf, _T("Loudness")))  return MIXERCONTROL_CONTROLTYPE_LOUDNESS;
		if (!_tcsicmp(aBuf, _T("StereoEnh"))) return MIXERCONTROL_CONTROLTYPE_STEREOENH;
		if (!_tcsicmp(aBuf, _T("BassBoost"))) return MIXERCONTROL_CONTROLTYPE_BASS_BOOST;
		if (!_tcsicmp(aBuf, _T("Pan")))       return MIXERCONTROL_CONTROLTYPE_PAN;
		if (!_tcsicmp(aBuf, _T("QSoundPan"))) return MIXERCONTROL_CONTROLTYPE_QSOUNDPAN;
		if (!_tcsicmp(aBuf, _T("Bass")))      return MIXERCONTROL_CONTROLTYPE_BASS;
		if (!_tcsicmp(aBuf, _T("Treble")))    return MIXERCONTROL_CONTROLTYPE_TREBLE;
		if (!_tcsicmp(aBuf, _T("Equalizer"))) return MIXERCONTROL_CONTROLTYPE_EQUALIZER;
		#define MIXERCONTROL_CONTROLTYPE_INVALID 0xFFFFFFFF // 0 might be a valid type, so use something definitely undefined.
		return MIXERCONTROL_CONTROLTYPE_INVALID;
	}

	static TitleMatchModes ConvertTitleMatchMode(LPTSTR aBuf)
	{
		if (!aBuf || !*aBuf) return MATCHMODE_INVALID;
		if (*aBuf == '1' && !*(aBuf + 1)) return FIND_IN_LEADING_PART;
		if (*aBuf == '2' && !*(aBuf + 1)) return FIND_ANYWHERE;
		if (*aBuf == '3' && !*(aBuf + 1)) return FIND_EXACT;
		if (!_tcsicmp(aBuf, _T("RegEx"))) return FIND_REGEX; // Goes with the above, not fast/slow below.

		if (!_tcsicmp(aBuf, _T("FAST"))) return FIND_FAST;
		if (!_tcsicmp(aBuf, _T("SLOW"))) return FIND_SLOW;
		return MATCHMODE_INVALID;
	}

#ifndef MINIDLL
	static MenuCommands ConvertMenuCommand(LPTSTR aBuf)
	{
		if (!aBuf || !*aBuf) return MENU_CMD_INVALID;
		if (!_tcsicmp(aBuf, _T("Show"))) return MENU_CMD_SHOW;
		if (!_tcsicmp(aBuf, _T("UseErrorLevel"))) return MENU_CMD_USEERRORLEVEL;
		if (!_tcsicmp(aBuf, _T("Add"))) return MENU_CMD_ADD;
		if (!_tcsicmp(aBuf, _T("Rename"))) return MENU_CMD_RENAME;
		if (!_tcsicmp(aBuf, _T("Check"))) return MENU_CMD_CHECK;
		if (!_tcsicmp(aBuf, _T("Uncheck"))) return MENU_CMD_UNCHECK;
		if (!_tcsicmp(aBuf, _T("ToggleCheck"))) return MENU_CMD_TOGGLECHECK;
		if (!_tcsicmp(aBuf, _T("Enable"))) return MENU_CMD_ENABLE;
		if (!_tcsicmp(aBuf, _T("Disable"))) return MENU_CMD_DISABLE;
		if (!_tcsicmp(aBuf, _T("ToggleEnable"))) return MENU_CMD_TOGGLEENABLE;
		if (!_tcsicmp(aBuf, _T("Standard"))) return MENU_CMD_STANDARD;
		if (!_tcsicmp(aBuf, _T("NoStandard"))) return MENU_CMD_NOSTANDARD;
		if (!_tcsicmp(aBuf, _T("Color"))) return MENU_CMD_COLOR;
		if (!_tcsicmp(aBuf, _T("Default"))) return MENU_CMD_DEFAULT;
		if (!_tcsicmp(aBuf, _T("NoDefault"))) return MENU_CMD_NODEFAULT;
		if (!_tcsicmp(aBuf, _T("Delete"))) return MENU_CMD_DELETE;
		if (!_tcsicmp(aBuf, _T("DeleteAll"))) return MENU_CMD_DELETEALL;
		if (!_tcsicmp(aBuf, _T("Tip"))) return MENU_CMD_TIP;
		if (!_tcsicmp(aBuf, _T("Icon"))) return MENU_CMD_ICON;
		if (!_tcsicmp(aBuf, _T("NoIcon"))) return MENU_CMD_NOICON;
		if (!_tcsicmp(aBuf, _T("Click"))) return MENU_CMD_CLICK;
		if (!_tcsicmp(aBuf, _T("MainWindow"))) return MENU_CMD_MAINWINDOW;
		if (!_tcsicmp(aBuf, _T("NoMainWindow"))) return MENU_CMD_NOMAINWINDOW;
		return MENU_CMD_INVALID;
	}
	
	static void ConvertGuiName(LPTSTR aBuf, LPTSTR &aCommand, LPTSTR *aName = NULL, size_t *aNameLength = NULL)
	{
		LPTSTR colon_pos;
		// Check for '+' and '-' to avoid ambiguity with something like "gui +Delimiter:".
		if (*aBuf == '+' || *aBuf == '-' || !(colon_pos = _tcschr(aBuf, ':'))) // Assignment.
		{
			aCommand = aBuf;
			// Name not specified, so leave it at the default set by caller.
			return;
		}

		size_t name_length = colon_pos - aBuf;
	
		// For backward compatibility, "01" to "09" must be treated as "1" to "9".
		if (name_length == 2 && *aBuf == '0' && aBuf[1] >= '1' && aBuf[1] <= '9')
		{
			// Normalize the number by excluding its leading "0".
			++aBuf;
			--name_length;
		}
	
		if (aName)
			*aName = aBuf;
		if (aNameLength)
			*aNameLength = name_length;
		aCommand = omit_leading_whitespace(colon_pos + 1);
	}

	static GuiCommands ConvertGuiCommand(LPTSTR aBuf)
	{
		if (!*aBuf || *aBuf == '+' || *aBuf == '-') // Assume a var ref that resolves to blank is "options" (for runtime flexibility).
			return GUI_CMD_OPTIONS;
		if (!_tcsicmp(aBuf, _T("Add"))) return GUI_CMD_ADD;
		if (!_tcsicmp(aBuf, _T("Show"))) return GUI_CMD_SHOW;
		if (!_tcsicmp(aBuf, _T("Submit"))) return GUI_CMD_SUBMIT;
		if (!_tcsicmp(aBuf, _T("Cancel")) || !_tcsicmp(aBuf, _T("Hide"))) return GUI_CMD_CANCEL;
		if (!_tcsicmp(aBuf, _T("Minimize"))) return GUI_CMD_MINIMIZE;
		if (!_tcsicmp(aBuf, _T("Maximize"))) return GUI_CMD_MAXIMIZE;
		if (!_tcsicmp(aBuf, _T("Restore"))) return GUI_CMD_RESTORE;
		if (!_tcsicmp(aBuf, _T("Destroy"))) return GUI_CMD_DESTROY;
		if (!_tcsicmp(aBuf, _T("Margin"))) return GUI_CMD_MARGIN;
		if (!_tcsicmp(aBuf, _T("Menu"))) return GUI_CMD_MENU;
		if (!_tcsicmp(aBuf, _T("Font"))) return GUI_CMD_FONT;
		if (!_tcsicmp(aBuf, _T("Tab"))) return GUI_CMD_TAB;
		if (!_tcsicmp(aBuf, _T("ListView"))) return GUI_CMD_LISTVIEW;
		if (!_tcsicmp(aBuf, _T("TreeView"))) return GUI_CMD_TREEVIEW;
		if (!_tcsicmp(aBuf, _T("Default"))) return GUI_CMD_DEFAULT;
		if (!_tcsicmp(aBuf, _T("Color"))) return GUI_CMD_COLOR;
		if (!_tcsicmp(aBuf, _T("Flash"))) return GUI_CMD_FLASH;
		if (!_tcsicmp(aBuf, _T("New"))) return GUI_CMD_NEW;
		return GUI_CMD_INVALID;
	}

	static GuiControlCmds ConvertGuiControlCmd(LPTSTR aBuf)
	{
		// If it's blank without a deref, that's CONTENTS.  Otherwise, assume it's OPTIONS for better
		// runtime flexibility (i.e. user can leave the variable blank to make the command do nothing).
		// Fix for v1.0.40.11: Since the above is counterintuitive and undocumented, it has been fixed
		// to behave the way most users would expect; that is, the contents of any deref in parameter 1
		// will behave the same as when such contents is present literally as parameter 1.  Another
		// reason for doing this is that otherwise, there is no way to specify the CONTENTS sub-command
		// in a variable.  For example, the following wouldn't work:
		// GuiControl, %WindowNumber%:, ...
		// GuiControl, %WindowNumberWithColon%, ...
		if (!*aBuf)
			return GUICONTROL_CMD_CONTENTS;
		if (*aBuf == '+' || *aBuf == '-') // Assume a var ref that resolves to blank is "options" (for runtime flexibility).
			return GUICONTROL_CMD_OPTIONS;
		if (!_tcsicmp(aBuf, _T("Text"))) return GUICONTROL_CMD_TEXT;
		if (!_tcsicmp(aBuf, _T("Move"))) return GUICONTROL_CMD_MOVE;
		if (!_tcsicmp(aBuf, _T("MoveDraw"))) return GUICONTROL_CMD_MOVEDRAW;
		if (!_tcsicmp(aBuf, _T("Focus"))) return GUICONTROL_CMD_FOCUS;
		if (!_tcsicmp(aBuf, _T("Choose"))) return GUICONTROL_CMD_CHOOSE;
		if (!_tcsicmp(aBuf, _T("ChooseString"))) return GUICONTROL_CMD_CHOOSESTRING;
		if (!_tcsicmp(aBuf, _T("Font"))) return GUICONTROL_CMD_FONT;

		// v1.0.38.02: Anything not already returned from above supports an optional boolean suffix.
		// The following example would hide the control: GuiControl, Show%VarContainingFalse%, MyControl
		// To support hex (due to the 'x' in it), search from the left rather than the right for the
		// first digit:
		LPTSTR suffix;
		for (suffix = aBuf; *suffix && !_istdigit(*suffix); ++suffix);
		bool invert = (*suffix ? !ATOI(suffix) : false);
		if (!_tcsnicmp(aBuf, _T("Enable"), 6)) return invert ? GUICONTROL_CMD_DISABLE : GUICONTROL_CMD_ENABLE;
		if (!_tcsnicmp(aBuf, _T("Disable"), 7)) return invert ? GUICONTROL_CMD_ENABLE : GUICONTROL_CMD_DISABLE;
		if (!_tcsnicmp(aBuf, _T("Show"), 4)) return invert ? GUICONTROL_CMD_HIDE : GUICONTROL_CMD_SHOW;
		if (!_tcsnicmp(aBuf, _T("Hide"), 4)) return invert ? GUICONTROL_CMD_SHOW : GUICONTROL_CMD_HIDE;

		return GUICONTROL_CMD_INVALID;
	}

	static GuiControlGetCmds ConvertGuiControlGetCmd(LPTSTR aBuf)
	{
		if (!*aBuf) return GUICONTROLGET_CMD_CONTENTS; // The implicit command when nothing was specified.
		if (!_tcsicmp(aBuf, _T("Pos"))) return GUICONTROLGET_CMD_POS;
		if (!_tcsicmp(aBuf, _T("Focus"))) return GUICONTROLGET_CMD_FOCUS;
		if (!_tcsicmp(aBuf, _T("FocusV"))) return GUICONTROLGET_CMD_FOCUSV; // Returns variable vs. ClassNN.
		if (!_tcsicmp(aBuf, _T("Enabled"))) return GUICONTROLGET_CMD_ENABLED;
		if (!_tcsicmp(aBuf, _T("Visible"))) return GUICONTROLGET_CMD_VISIBLE;
		if (!_tcsicmp(aBuf, _T("Hwnd"))) return GUICONTROLGET_CMD_HWND;
		if (!_tcsicmp(aBuf, _T("Name"))) return GUICONTROLGET_CMD_NAME;
		return GUICONTROLGET_CMD_INVALID;
	}

	static GuiControls ConvertGuiControl(LPTSTR aBuf)
	{
		if (!aBuf || !*aBuf) return GUI_CONTROL_INVALID;
		if (!_tcsicmp(aBuf, _T("Text"))) return GUI_CONTROL_TEXT;
		if (!_tcsicmp(aBuf, _T("Edit"))) return GUI_CONTROL_EDIT;
		if (!_tcsicmp(aBuf, _T("Button"))) return GUI_CONTROL_BUTTON;
		if (!_tcsicmp(aBuf, _T("Checkbox"))) return GUI_CONTROL_CHECKBOX;
		if (!_tcsicmp(aBuf, _T("Radio"))) return GUI_CONTROL_RADIO;
		if (!_tcsicmp(aBuf, _T("DDL")) || !_tcsicmp(aBuf, _T("DropDownList"))) return GUI_CONTROL_DROPDOWNLIST;
		if (!_tcsicmp(aBuf, _T("ComboBox"))) return GUI_CONTROL_COMBOBOX;
		if (!_tcsicmp(aBuf, _T("ListBox"))) return GUI_CONTROL_LISTBOX;
		if (!_tcsicmp(aBuf, _T("ListView"))) return GUI_CONTROL_LISTVIEW;
		if (!_tcsicmp(aBuf, _T("TreeView"))) return GUI_CONTROL_TREEVIEW;
		// Keep those seldom used at the bottom for performance:
		if (!_tcsicmp(aBuf, _T("UpDown"))) return GUI_CONTROL_UPDOWN;
		if (!_tcsicmp(aBuf, _T("Slider"))) return GUI_CONTROL_SLIDER;
		if (!_tcsicmp(aBuf, _T("Progress"))) return GUI_CONTROL_PROGRESS;
		if (!_tcsicmp(aBuf, _T("Tab"))) return GUI_CONTROL_TAB;
		if (!_tcsicmp(aBuf, _T("Tab2"))) return GUI_CONTROL_TAB2; // v1.0.47.05: Used only temporarily: becomes TAB vs. TAB2 upon creation.
		if (!_tcsicmp(aBuf, _T("GroupBox"))) return GUI_CONTROL_GROUPBOX;
		if (!_tcsicmp(aBuf, _T("Pic")) || !_tcsicmp(aBuf, _T("Picture"))) return GUI_CONTROL_PIC;
		if (!_tcsicmp(aBuf, _T("DateTime"))) return GUI_CONTROL_DATETIME;
		if (!_tcsicmp(aBuf, _T("MonthCal"))) return GUI_CONTROL_MONTHCAL;
		if (!_tcsicmp(aBuf, _T("Hotkey"))) return GUI_CONTROL_HOTKEY;
		if (!_tcsicmp(aBuf, _T("StatusBar"))) return GUI_CONTROL_STATUSBAR;
		if (!_tcsicmp(aBuf, _T("ActiveX"))) return GUI_CONTROL_ACTIVEX;
		if (!_tcsicmp(aBuf, _T("Link"))) return GUI_CONTROL_LINK;
		if (!_tcsicmp(aBuf, _T("Custom"))) return GUI_CONTROL_CUSTOM;
		if (!_tcsicmp(aBuf, _T("Gui"))) return GUI_CONTROL_GUI;
		return GUI_CONTROL_INVALID;
	}
#endif

	static ThreadCommands ConvertThreadCommand(LPTSTR aBuf)
	{
		if (!aBuf || !*aBuf) return THREAD_CMD_INVALID;
		if (!_tcsicmp(aBuf, _T("Priority"))) return THREAD_CMD_PRIORITY;
		if (!_tcsicmp(aBuf, _T("Interrupt"))) return THREAD_CMD_INTERRUPT;
		if (!_tcsicmp(aBuf, _T("NoTimers"))) return THREAD_CMD_NOTIMERS;
		return THREAD_CMD_INVALID;
	}
	
	static ProcessCmds ConvertProcessCmd(LPTSTR aBuf)
	{
		if (!aBuf || !*aBuf) return PROCESS_CMD_INVALID;
		if (!_tcsicmp(aBuf, _T("Exist"))) return PROCESS_CMD_EXIST;
		if (!_tcsicmp(aBuf, _T("Close"))) return PROCESS_CMD_CLOSE;
		if (!_tcsicmp(aBuf, _T("Wait"))) return PROCESS_CMD_WAIT;
		if (!_tcsicmp(aBuf, _T("WaitClose"))) return PROCESS_CMD_WAITCLOSE;
		return PROCESS_CMD_INVALID;
	}

	static ControlCmds ConvertControlCmd(LPTSTR aBuf)
	{
		if (!aBuf || !*aBuf) return CONTROL_CMD_INVALID;
		if (!_tcsicmp(aBuf, _T("Check"))) return CONTROL_CMD_CHECK;
		if (!_tcsicmp(aBuf, _T("Uncheck"))) return CONTROL_CMD_UNCHECK;
		if (!_tcsicmp(aBuf, _T("Enable"))) return CONTROL_CMD_ENABLE;
		if (!_tcsicmp(aBuf, _T("Disable"))) return CONTROL_CMD_DISABLE;
		if (!_tcsicmp(aBuf, _T("Show"))) return CONTROL_CMD_SHOW;
		if (!_tcsicmp(aBuf, _T("Hide"))) return CONTROL_CMD_HIDE;
		if (!_tcsicmp(aBuf, _T("Style"))) return CONTROL_CMD_STYLE;
		if (!_tcsicmp(aBuf, _T("ExStyle"))) return CONTROL_CMD_EXSTYLE;
		if (!_tcsicmp(aBuf, _T("ShowDropDown"))) return CONTROL_CMD_SHOWDROPDOWN;
		if (!_tcsicmp(aBuf, _T("HideDropDown"))) return CONTROL_CMD_HIDEDROPDOWN;
		if (!_tcsicmp(aBuf, _T("TabLeft"))) return CONTROL_CMD_TABLEFT;
		if (!_tcsicmp(aBuf, _T("TabRight"))) return CONTROL_CMD_TABRIGHT;
		if (!_tcsicmp(aBuf, _T("Add"))) return CONTROL_CMD_ADD;
		if (!_tcsicmp(aBuf, _T("Delete"))) return CONTROL_CMD_DELETE;
		if (!_tcsicmp(aBuf, _T("Choose"))) return CONTROL_CMD_CHOOSE;
		if (!_tcsicmp(aBuf, _T("ChooseString"))) return CONTROL_CMD_CHOOSESTRING;
		if (!_tcsicmp(aBuf, _T("EditPaste"))) return CONTROL_CMD_EDITPASTE;
		return CONTROL_CMD_INVALID;
	}

	static ControlGetCmds ConvertControlGetCmd(LPTSTR aBuf)
	{
		if (!aBuf || !*aBuf) return CONTROLGET_CMD_INVALID;
		if (!_tcsicmp(aBuf, _T("Checked"))) return CONTROLGET_CMD_CHECKED;
		if (!_tcsicmp(aBuf, _T("Enabled"))) return CONTROLGET_CMD_ENABLED;
		if (!_tcsicmp(aBuf, _T("Visible"))) return CONTROLGET_CMD_VISIBLE;
		if (!_tcsicmp(aBuf, _T("Tab"))) return CONTROLGET_CMD_TAB;
		if (!_tcsicmp(aBuf, _T("FindString"))) return CONTROLGET_CMD_FINDSTRING;
		if (!_tcsicmp(aBuf, _T("Choice"))) return CONTROLGET_CMD_CHOICE;
		if (!_tcsicmp(aBuf, _T("List"))) return CONTROLGET_CMD_LIST;
		if (!_tcsicmp(aBuf, _T("LineCount"))) return CONTROLGET_CMD_LINECOUNT;
		if (!_tcsicmp(aBuf, _T("CurrentLine"))) return CONTROLGET_CMD_CURRENTLINE;
		if (!_tcsicmp(aBuf, _T("CurrentCol"))) return CONTROLGET_CMD_CURRENTCOL;
		if (!_tcsicmp(aBuf, _T("Line"))) return CONTROLGET_CMD_LINE;
		if (!_tcsicmp(aBuf, _T("Selected"))) return CONTROLGET_CMD_SELECTED;
		if (!_tcsicmp(aBuf, _T("Style"))) return CONTROLGET_CMD_STYLE;
		if (!_tcsicmp(aBuf, _T("ExStyle"))) return CONTROLGET_CMD_EXSTYLE;
		if (!_tcsicmp(aBuf, _T("Hwnd"))) return CONTROLGET_CMD_HWND;
		return CONTROLGET_CMD_INVALID;
	}

	static DriveCmds ConvertDriveCmd(LPTSTR aBuf)
	{
		if (!aBuf || !*aBuf) return DRIVE_CMD_INVALID;
		if (!_tcsicmp(aBuf, _T("Eject"))) return DRIVE_CMD_EJECT;
		if (!_tcsicmp(aBuf, _T("Lock"))) return DRIVE_CMD_LOCK;
		if (!_tcsicmp(aBuf, _T("Unlock"))) return DRIVE_CMD_UNLOCK;
		if (!_tcsicmp(aBuf, _T("Label"))) return DRIVE_CMD_LABEL;
		return DRIVE_CMD_INVALID;
	}

	static DriveGetCmds ConvertDriveGetCmd(LPTSTR aBuf)
	{
		if (!aBuf || !*aBuf) return DRIVEGET_CMD_INVALID;
		if (!_tcsicmp(aBuf, _T("List"))) return DRIVEGET_CMD_LIST;
		if (!_tcsicmp(aBuf, _T("FileSystem")) || !_tcsicmp(aBuf, _T("FS"))) return DRIVEGET_CMD_FILESYSTEM;
		if (!_tcsicmp(aBuf, _T("Label"))) return DRIVEGET_CMD_LABEL;
		if (!_tcsicmp(aBuf, _T("Serial"))) return DRIVEGET_CMD_SERIAL;
		if (!_tcsicmp(aBuf, _T("Type"))) return DRIVEGET_CMD_TYPE;
		if (!_tcsicmp(aBuf, _T("Status"))) return DRIVEGET_CMD_STATUS;
		if (!_tcsicmp(aBuf, _T("StatusCD"))) return DRIVEGET_CMD_STATUSCD;
		if (!_tcsicmp(aBuf, _T("Capacity")) || !_tcsicmp(aBuf, _T("Cap"))) return DRIVEGET_CMD_CAPACITY;
		if (!_tcsicmp(aBuf, _T("SpaceFree"))) return DRIVEGET_CMD_SPACEFREE;
		return DRIVEGET_CMD_INVALID;
	}

	static ToggleValueType ConvertOnOff(LPTSTR aBuf, ToggleValueType aDefault = TOGGLE_INVALID)
	// Returns aDefault if aBuf isn't either ON, OFF, or blank.
	{
		if (!aBuf || !*aBuf) return NEUTRAL;
		if (!_tcsicmp(aBuf, _T("On")) || !_tcscmp(aBuf, _T("1"))) return TOGGLED_ON;
		if (!_tcsicmp(aBuf, _T("Off")) || !_tcscmp(aBuf, _T("0"))) return TOGGLED_OFF;
		return aDefault;
	}

	static ToggleValueType ConvertOnOffAlways(LPTSTR aBuf, ToggleValueType aDefault = TOGGLE_INVALID)
	// Returns aDefault if aBuf isn't either ON, OFF, ALWAYSON, ALWAYSOFF, or blank.
	{
		if (ToggleValueType toggle = ConvertOnOff(aBuf))
			return toggle;
		if (!_tcsicmp(aBuf, _T("AlwaysOn"))) return ALWAYS_ON;
		if (!_tcsicmp(aBuf, _T("AlwaysOff"))) return ALWAYS_OFF;
		return aDefault;
	}

	static ToggleValueType ConvertOnOffToggle(LPTSTR aBuf, ToggleValueType aDefault = TOGGLE_INVALID)
	// Returns aDefault if aBuf isn't either ON, OFF, TOGGLE, or blank.
	{
		if (ToggleValueType toggle = ConvertOnOff(aBuf))
			return toggle;
		if (!_tcsicmp(aBuf, _T("Toggle")) || !_tcscmp(aBuf, _T("-1"))) return TOGGLE;
		return aDefault;
	}

	static StringCaseSenseType ConvertStringCaseSense(LPTSTR aBuf)
	{
		if (!_tcsicmp(aBuf, _T("On"))) return SCS_SENSITIVE;
		if (!_tcsicmp(aBuf, _T("Off"))) return SCS_INSENSITIVE;
		if (!_tcsicmp(aBuf, _T("Locale"))) return SCS_INSENSITIVE_LOCALE;
		return SCS_INVALID;
	}

	static ToggleValueType ConvertOnOffTogglePermit(LPTSTR aBuf, ToggleValueType aDefault = TOGGLE_INVALID)
	// Returns aDefault if aBuf isn't either ON, OFF, TOGGLE, PERMIT, or blank.
	{
		if (ToggleValueType toggle = ConvertOnOffToggle(aBuf))
			return toggle;
		if (!_tcsicmp(aBuf, _T("Permit"))) return TOGGLE_PERMIT;
		return aDefault;
	}

	static ToggleValueType ConvertBlockInput(LPTSTR aBuf)
	{
		if (ToggleValueType toggle = ConvertOnOff(aBuf))
			return toggle;
		if (!_tcsicmp(aBuf, _T("Send"))) return TOGGLE_SEND;
		if (!_tcsicmp(aBuf, _T("Mouse"))) return TOGGLE_MOUSE;
		if (!_tcsicmp(aBuf, _T("SendAndMouse"))) return TOGGLE_SENDANDMOUSE;
		if (!_tcsicmp(aBuf, _T("Default"))) return TOGGLE_DEFAULT;
		if (!_tcsicmp(aBuf, _T("MouseMove"))) return TOGGLE_MOUSEMOVE;
		if (!_tcsicmp(aBuf, _T("MouseMoveOff"))) return TOGGLE_MOUSEMOVEOFF;
		return TOGGLE_INVALID;
	}

	static SendModes ConvertSendMode(LPTSTR aBuf, SendModes aValueToReturnIfInvalid)
	{
		if (!_tcsicmp(aBuf, _T("Play"))) return SM_PLAY;
		if (!_tcsicmp(aBuf, _T("Event"))) return SM_EVENT;
		if (!_tcsnicmp(aBuf, _T("Input"), 5)) // This IF must be listed last so that it can fall through to bottom line.
		{
			aBuf += 5;
			if (!*aBuf || !_tcsicmp(aBuf, _T("ThenEvent"))) // "ThenEvent" is supported for backward compatibility with 1.0.43.00.
				return SM_INPUT;
			if (!_tcsicmp(aBuf, _T("ThenPlay")))
				return SM_INPUT_FALLBACK_TO_PLAY;
			//else fall through and return the indication of invalidity.
		}
		return aValueToReturnIfInvalid;
	}

	static FileLoopModeType ConvertLoopMode(LPTSTR aBuf)
	// Returns the file loop mode, or FILE_LOOP_INVALID if aBuf contains an invalid mode.
	{
		for (FileLoopModeType mode = FILE_LOOP_INVALID;;)
		{
			switch (ctoupper(*aBuf++))
			{
			// For simplicity, both are allowed with either kind of loop:
			case 'F': // Files
			case 'V': // Values
				mode |= FILE_LOOP_FILES_ONLY;
				break;
			case 'D': // Directories
			case 'K': // Keys
				mode |= FILE_LOOP_FOLDERS_ONLY;
				break;
			case 'R':
				mode |= FILE_LOOP_RECURSE;
				break;
			case ' ':  // Allow whitespace.
			case '\t': //
				break;
			case '\0':
				if ((mode & FILE_LOOP_FILES_AND_FOLDERS) == 0)
					mode |= FILE_LOOP_FILES_ONLY; // Set default.
				return mode;
			default: // Invalid character.
				return FILE_LOOP_INVALID;
			}
		}
	}

	static int ConvertRunMode(LPTSTR aBuf)
	// Returns the matching WinShow mode, or SW_SHOWNORMAL if none.
	// These are also the modes that AutoIt3 uses.
	{
		// For v1.0.19, this was made more permissive (the use of strcasestr vs. stricmp) to support
		// the optional word UseErrorLevel inside this parameter:
		if (!aBuf || !*aBuf) return SW_SHOWNORMAL;
		if (tcscasestr(aBuf, _T("MIN"))) return SW_MINIMIZE;
		if (tcscasestr(aBuf, _T("MAX"))) return SW_MAXIMIZE;
		if (tcscasestr(aBuf, _T("HIDE"))) return SW_HIDE;
		return SW_SHOWNORMAL;
	}

	static int ConvertMouseButton(LPTSTR aBuf, bool aAllowWheel = true, bool aUseLogicalButton = false)
	// Returns the matching VK, or zero if none.
	{
		if (!*aBuf || !_tcsicmp(aBuf, _T("LEFT")) || !_tcsicmp(aBuf, _T("L")))
			return aUseLogicalButton ? VK_LBUTTON_LOGICAL : VK_LBUTTON; // Some callers rely on this default when !*aBuf.
		if (!_tcsicmp(aBuf, _T("RIGHT")) || !_tcsicmp(aBuf, _T("R"))) return aUseLogicalButton ? VK_RBUTTON_LOGICAL : VK_RBUTTON;
		if (!_tcsicmp(aBuf, _T("MIDDLE")) || !_tcsicmp(aBuf, _T("M"))) return VK_MBUTTON;
		if (!_tcsicmp(aBuf, _T("X1"))) return VK_XBUTTON1;
		if (!_tcsicmp(aBuf, _T("X2"))) return VK_XBUTTON2;
		if (aAllowWheel)
		{
			if (!_tcsicmp(aBuf, _T("WheelUp")) || !_tcsicmp(aBuf, _T("WU"))) return VK_WHEEL_UP;
			if (!_tcsicmp(aBuf, _T("WheelDown")) || !_tcsicmp(aBuf, _T("WD"))) return VK_WHEEL_DOWN;
			// Lexikos: Support horizontal scrolling in Windows Vista and later.
			if (!_tcsicmp(aBuf, _T("WheelLeft")) || !_tcsicmp(aBuf, _T("WL"))) return VK_WHEEL_LEFT;
			if (!_tcsicmp(aBuf, _T("WheelRight")) || !_tcsicmp(aBuf, _T("WR"))) return VK_WHEEL_RIGHT;
		}
		return 0;
	}

	static CoordModeType ConvertCoordMode(LPTSTR aBuf)
	{
		if (!aBuf || !*aBuf || !_tcsicmp(aBuf, _T("Screen")))
			return COORD_MODE_SCREEN;
		else if (!_tcsicmp(aBuf, _T("Relative")) || !_tcsicmp(aBuf, _T("Window")))
			return COORD_MODE_WINDOW;
		else if (!_tcsicmp(aBuf, _T("Client")))
			return COORD_MODE_CLIENT;
		return -1;
	}

	static CoordModeType ConvertCoordModeCmd(LPTSTR aBuf)
	{
		if (!aBuf || !*aBuf) return -1;
#ifndef MINIDLL
		if (!_tcsicmp(aBuf, _T("Pixel"))) return COORD_MODE_PIXEL;
#endif
		if (!_tcsicmp(aBuf, _T("Mouse"))) return COORD_MODE_MOUSE;
		if (!_tcsicmp(aBuf, _T("ToolTip"))) return COORD_MODE_TOOLTIP;
		if (!_tcsicmp(aBuf, _T("Caret"))) return COORD_MODE_CARET;
#ifndef MINIDLL
		if (!_tcsicmp(aBuf, _T("Menu"))) return COORD_MODE_MENU;
#endif
		return -1;
	}

	static VariableTypeType ConvertVariableTypeName(LPTSTR aBuf)
	// Returns the matching type, or zero if none.
	{
		if (!aBuf || !*aBuf) return VAR_TYPE_INVALID;
		if (!_tcsicmp(aBuf, _T("Integer"))) return VAR_TYPE_INTEGER;
		if (!_tcsicmp(aBuf, _T("Float"))) return VAR_TYPE_FLOAT;
		if (!_tcsicmp(aBuf, _T("Number"))) return VAR_TYPE_NUMBER;
		if (!_tcsicmp(aBuf, _T("Object"))) return VAR_TYPE_OBJECT; // v2
		if (!_tcsicmp(aBuf, _T("ByRef"))) return VAR_TYPE_BYREF; // v2
		if (!_tcsicmp(aBuf, _T("Time"))) return VAR_TYPE_TIME;
		if (!_tcsicmp(aBuf, _T("Date"))) return VAR_TYPE_TIME;  // "date" is just an alias for "time".
		if (!_tcsicmp(aBuf, _T("Digit"))) return VAR_TYPE_DIGIT;
		if (!_tcsicmp(aBuf, _T("Xdigit"))) return VAR_TYPE_XDIGIT;
		if (!_tcsicmp(aBuf, _T("Alnum"))) return VAR_TYPE_ALNUM;
		if (!_tcsicmp(aBuf, _T("Alpha"))) return VAR_TYPE_ALPHA;
		if (!_tcsicmp(aBuf, _T("Upper"))) return VAR_TYPE_UPPER;
		if (!_tcsicmp(aBuf, _T("Lower"))) return VAR_TYPE_LOWER;
		if (!_tcsicmp(aBuf, _T("Space"))) return VAR_TYPE_SPACE;
		return VAR_TYPE_INVALID;
	}

	static UINT ConvertFileEncoding(LPTSTR aBuf)
	// Returns the encoding with possible CP_AHKNOBOM flag, or (UINT)-1 if invalid.
	{
		if (!aBuf || !*aBuf)
			// Active codepage, equivalent to specifying CP0.
			return CP_ACP;
		if (!_tcsicmp(aBuf, _T("UTF-8")))		return CP_UTF8;
		if (!_tcsicmp(aBuf, _T("UTF-8-RAW")))	return CP_UTF8 | CP_AHKNOBOM;
		if (!_tcsicmp(aBuf, _T("UTF-16")))		return 1200;
		if (!_tcsicmp(aBuf, _T("UTF-16-RAW")))	return 1200 | CP_AHKNOBOM;
		if (!_tcsnicmp(aBuf, _T("CP"), 2) && IsNumeric(aBuf + 2, false, false))
			// CPnnn
			return ATOU(aBuf + 2);
		return -1;
	}

	static ResultType ValidateMouseCoords(LPTSTR aX, LPTSTR aY)
	{
		// OK: Both are absent, which is the signal to use the current position.
		// OK: Both are present (that they are numeric is validated elsewhere).
		// FAIL: One is absent but the other is present.
		return (!*aX && !*aY) || (*aX && *aY) ? OK : FAIL;
	}

	bool IsExemptFromSuspend()
	{
		// Hotkey and Hotstring subroutines whose first line is the Suspend command are exempt from
		// being suspended themselves except when their first parameter is the literal
		// word "on":
		return mActionType == ACT_SUSPEND && (!mArgc || ArgHasDeref(1) || _tcsicmp(mArg[0].text, _T("On")));
	}

	static LPTSTR LogToText(LPTSTR aBuf, int aBufSize);
	LPTSTR VicinityToText(LPTSTR aBuf, int aBufSize);
	LPTSTR ToText(LPTSTR aBuf, int aBufSize, bool aCRLF, DWORD aElapsed = 0, bool aLineWasResumed = false);

	static void ToggleSuspendState();
	static void PauseUnderlyingThread(bool aTrueForPauseFalseForUnpause);
	ResultType ChangePauseState(ToggleValueType aChangeTo, bool aAlwaysOperateOnUnderlyingThread);
	static ResultType ScriptBlockInput(bool aEnable);

	Line *PreparseError(LPTSTR aErrorText, LPTSTR aExtraInfo = _T(""));
	// Call this LineError to avoid confusion with Script's error-displaying functions:
	ResultType LineError(LPCTSTR aErrorText, ResultType aErrorType = FAIL, LPCTSTR aExtraInfo = _T(""));
	static int FormatError(LPTSTR aBuf, int aBufSize, ResultType aErrorType, LPCTSTR aErrorText, LPCTSTR aExtraInfo, Line *aLine, LPCTSTR aFooter);
	IObject *CreateRuntimeException(LPCTSTR aErrorText, LPCTSTR aWhat = NULL, LPCTSTR aExtraInfo = _T(""));
	ResultType ThrowRuntimeException(LPCTSTR aErrorText, LPCTSTR aWhat = NULL, LPCTSTR aExtraInfo = _T(""));
	
	ResultType SetErrorsOrThrow(bool aError, DWORD aLastErrorOverride = -1);
	ResultType SetErrorLevelOrThrow() { return SetErrorLevelOrThrowBool(true); }
	ResultType SetErrorLevelOrThrowBool(bool aError);        //
	ResultType SetErrorLevelOrThrowStr(LPCTSTR aErrorValue); // Explicit names to avoid calling the wrong overload.
	ResultType SetErrorLevelOrThrowInt(int aErrorValue);     //

	Line(FileIndexType aFileIndex, LineNumberType aFileLineNumber, ActionTypeType aActionType
		, ArgStruct aArg[], ArgCountType aArgc) // Constructor
		: mFileIndex(aFileIndex), mLineNumber(aFileLineNumber), mActionType(aActionType)
		, mAttribute(ATTR_NONE), mArgc(aArgc), mArg(aArg)
		, mPrevLine(NULL), mNextLine(NULL), mRelatedLine(NULL), mParentLine(NULL)
#ifdef CONFIG_DEBUGGER
		, mBreakpoint(NULL)
#endif
		{}
	void *operator new(size_t aBytes) {return SimpleHeap::Malloc(aBytes);}
	void *operator new[](size_t aBytes) {return SimpleHeap::Malloc(aBytes);}
	void operator delete(void *aPtr) {}  // Intentionally does nothing because we're using SimpleHeap for everything.
	void operator delete[](void *aPtr) {}

	// AutoIt3 functions:
	static bool Util_CopyDir(LPCTSTR szInputSource, LPCTSTR szInputDest, bool bOverwrite);
	static bool Util_MoveDir(LPCTSTR szInputSource, LPCTSTR szInputDest, int OverwriteMode);
	static bool Util_RemoveDir(LPCTSTR szInputSource, bool bRecurse);
	static int Util_CopyFile(LPCTSTR szInputSource, LPCTSTR szInputDest, bool bOverwrite, bool bMove, DWORD &aLastError);
	static void Util_ExpandFilenameWildcard(LPCTSTR szSource, LPCTSTR szDest, LPTSTR szExpandedDest);
	static void Util_ExpandFilenameWildcardPart(LPCTSTR szSource, LPCTSTR szDest, LPTSTR szExpandedDest);
	static bool Util_CreateDir(LPCTSTR szDirName);
	static bool Util_DoesFileExist(LPCTSTR szFilename);
	static bool Util_IsDir(LPCTSTR szPath);
	static void Util_GetFullPathName(LPCTSTR szIn, LPTSTR szOut);
	static bool Util_IsDifferentVolumes(LPCTSTR szPath1, LPCTSTR szPath2);
};



class Label : public IObjectComCompatible
{
public:
	LPTSTR mName;
	Line *mJumpToLine;
	Label *mPrevLabel, *mNextLabel;  // Prev & Next items in linked list.
#ifndef MINIDLL
	bool IsExemptFromSuspend()
	{
		// See Line::IsExemptFromSuspend() for comments.
		return mJumpToLine->IsExemptFromSuspend();
	}
#endif
	ResultType Execute()
	// This function was added in v1.0.46.16 to support A_ThisLabel.
	{
		Label *prev_label =g->CurrentLabel; // This will be non-NULL when a subroutine is called from inside another subroutine.
		g->CurrentLabel = this;
		ResultType result;
		DEBUGGER_STACK_PUSH(this)
		// I'm pretty sure it's not valid for the following call to ExecUntil() to tell us to jump
		// somewhere, because the called function, or a layer even deeper, should handle the goto
		// prior to returning to us?  So the last parameter is omitted:
		result = mJumpToLine->ExecUntil(UNTIL_RETURN); // The script loader has ensured that Label::mJumpToLine can't be NULL.
		DEBUGGER_STACK_POP()
		g->CurrentLabel = prev_label;
		return result;
	}

	Label(LPTSTR aLabelName)
		: mName(aLabelName) // Caller gave us a pointer to dynamic memory for this (or an empty string in the case of mPlaceholderLabel).
		, mJumpToLine(NULL)
		, mPrevLabel(NULL), mNextLabel(NULL)
	{}
	void *operator new(size_t aBytes) {return SimpleHeap::Malloc(aBytes);}
	void *operator new[](size_t aBytes) {return SimpleHeap::Malloc(aBytes);}
	void operator delete(void *aPtr) {}
	void operator delete[](void *aPtr) {}

	// IObject.
	ResultType STDMETHODCALLTYPE Invoke(ResultToken &aResultToken, ExprTokenType &aThisToken, int aFlags, ExprTokenType *aParam[], int aParamCount);
	ULONG STDMETHODCALLTYPE AddRef() { return 1; }
	ULONG STDMETHODCALLTYPE Release() { return 1; }
#ifdef CONFIG_DEBUGGER
	void DebugWriteProperty(IDebugProperties *, int aPage, int aPageSize, int aDepth) {}
#endif
};



// This class encapsulates a pointer to an object which can be called by a timer,
// hotkey, etc.  It provides common functionality that wouldn't be suitable for the
// base IObject interface, but is needed for detection of "Suspend" or "Critical"
// prior to calling the sub or function.
class LabelPtr
{
protected:
	IObject *mObject;
	
	enum CallableType
	{
		Callable_Label,
		Callable_Func,
		Callable_Object
	};
	static Line *getJumpToLine(IObject *aObject);
	static CallableType getType(IObject *aObject);

public:
	LabelPtr() : mObject(NULL) {}
	LabelPtr(IObject *object) : mObject(object) {}
	ResultType ExecuteInNewThread(TCHAR *aNewThreadDesc
		, ExprTokenType *aParamValue = NULL, int aParamCount = 0, INT_PTR *aRetVal = NULL) const;
	const LabelPtr* operator-> () { return this; } // Act like a pointer.
	operator void *() const { return mObject; } // For comparisons and boolean eval.

	Label *ToLabel() const { return getType(mObject) == Callable_Label ? (Label *)mObject : NULL; }
	
	// Helper methods for legacy code which deals with Labels.
	bool IsExemptFromSuspend() const;
	ActionTypeType TypeOfFirstLine() const;
	LPTSTR Name() const;
};

// LabelPtr with automatic reference-counting, for storing an object safely,
// such as in a HotkeyVariant, UserMenuItem, etc.  In future, this could be
// replaced with a more general smart pointer class.
class LabelRef : public LabelPtr
{
public:
	LabelRef() : LabelPtr() {}
	LabelRef(IObject *object) : LabelPtr(object)
	{
		if (object)
			object->AddRef();
	}
	LabelRef(const LabelPtr &other) : LabelPtr(other)
	{
		if (mObject)
			mObject->AddRef();
	}
	LabelRef & operator = (IObject *object)
	{
		if (object)
			object->AddRef();
		if (mObject)
			mObject->Release();
		mObject = object;
		return *this;
	}
	~LabelRef()
	{
		if (mObject)
			mObject->Release();
	}
};

#ifdef ENABLE_DLLCALL
#ifdef WIN32_PLATFORM
// Interface for DynaCall():
#define  DC_MICROSOFT           0x0000      // Default
#define  DC_BORLAND             0x0001      // Borland compat
#define  DC_CALL_CDECL          0x0010      // __cdecl
#define  DC_CALL_STD            0x0020      // __stdcall
#define  DC_RETVAL_MATH4        0x0100      // Return value in ST
#define  DC_RETVAL_MATH8        0x0200      // Return value in ST

#define  DC_CALL_STD_BO         (DC_CALL_STD | DC_BORLAND)
#define  DC_CALL_STD_MS         (DC_CALL_STD | DC_MICROSOFT)
#define  DC_CALL_STD_M8         (DC_CALL_STD | DC_RETVAL_MATH8)
#endif


union DYNARESULT                // Various result types
{
	int     Int;                // Generic four-byte type
	long    Long;               // Four-byte long
	void   *Pointer;            // 32-bit pointer
	float   Float;              // Four byte real
	double  Double;             // 8-byte real
	__int64 Int64;              // big int (64-bit)
	UINT_PTR UIntPtr;
};

////////////////////////
// DYNACALL TOKEN //
////////////////////////

struct DYNAPARM
{
	union
	{
		int value_int; // Args whose width is less than 32-bit are also put in here because they are right justified within a 32-bit block on the stack.
		float value_float;
		__int64 value_int64;
		UINT_PTR value_uintptr;
		double value_double;
		char *astr;
		wchar_t *wstr;
		void *ptr;
	};
	// Might help reduce struct size to keep other members last and adjacent to each other (due to
	// 8-byte alignment caused by the presence of double and __int64 members in the union above).
	DllArgTypes type;
	bool passed_by_address;
	bool is_unsigned; // Allows return value and output parameters to be interpreted as unsigned vs. signed.
};

void ConvertDllArgType(LPTSTR aBuf[], DYNAPARM &aDynaParam);
#endif

enum FuncParamDefaults {PARAM_DEFAULT_NONE, PARAM_DEFAULT_STR, PARAM_DEFAULT_INT, PARAM_DEFAULT_FLOAT};
struct FuncParam
{
	Var *var;
	WORD is_byref; // Boolean, but defined as WORD in case it helps data alignment and/or performance (BOOL vs. WORD didn't help benchmarks).
	WORD default_type;
	union {LPTSTR default_str; __int64 default_int64; double default_double;};
};

struct FuncResult : public ResultToken
{
	TCHAR mRetValBuf[MAX_NUMBER_SIZE];

	FuncResult()
	{
		InitResult(mRetValBuf);
	}
};


typedef BIF_DECL((* BuiltInFunctionType));

class Func : public IObjectComCompatible
{
public:
	LPTSTR mName;
	union {
		struct { // User-defined functions.
			Line *mJumpToLine;
			FuncParam *mParam; // Holds an array of FuncParams (array length: mParamCount).
			Object *mClass; // The class which this Func was defined in, if applicable.
		};
		struct { // Built-in functions.
			BuiltInFunctionType mBIF;
			UCHAR *mOutputVars; // String of indices indicating which params are output vars (for ACT_FUNC and BIF_PerformAction).
			BuiltInFunctionID mID; // For code sharing: this function's ID in the group of functions which share the same C++ function.
		};
	};
	int mParamCount; // The function's maximum number of parameters.  For UDFs, also the number of items in the mParam array.
	int mMinParams;  // The number of mandatory parameters (populated for both UDFs and built-in's).
	Label *mFirstLabel, *mLastLabel; // Linked list of private labels.
	Var **mGlobalVar; // Array of global declarations
	Var **mVar, **mLazyVar; // Array of pointers-to-variable, allocated upon first use and later expanded as needed.
	Var **mStaticVar, **mStaticLazyVar;
	// Count of items in the above array as well as the maximum capacity.
	int mGlobalVarCount, mVarCount, mVarCountMax, mLazyVarCount, mStaticVarCount, mStaticVarCountMax, mStaticLazyVarCount; 
	int mInstances; // How many instances currently exist on the call stack (due to recursion or thread interruption).  Future use: Might be used to limit how deep recursion can go to help prevent stack overflow.

	// Keep small members adjacent to each other to save space and improve perf. due to byte alignment:
	UCHAR mDefaultVarType;
	#define VAR_DECLARE_NONE   0
	#define VAR_DECLARE_GLOBAL (VAR_DECLARED | VAR_GLOBAL)
	#define VAR_DECLARE_SUPER_GLOBAL (VAR_DECLARE_GLOBAL | VAR_SUPER_GLOBAL)
	#define VAR_DECLARE_LOCAL  (VAR_DECLARED | VAR_LOCAL)
	#define VAR_DECLARE_STATIC (VAR_DECLARED | VAR_LOCAL | VAR_LOCAL_STATIC)

	bool mIsBuiltIn; // Determines contents of union. Keep this member adjacent/contiguous with the above.
	// Note that it's possible for a built-in function such as WinExist() to become a normal/UDF via
	// override in the script.  So mIsBuiltIn should always be used to determine whether the function
	// is truly built-in, not its name.
	bool mIsVariadic;
	bool mHasReturn; // Does the function require an output var to receive the return value in command syntax mode?

#define MAX_FUNC_OUTPUT_VAR 7
	bool ArgIsOutputVar(int aIndex)
	{
		if (!mIsBuiltIn)
			return aIndex <= mParamCount && mParam[aIndex].is_byref;
		if (!mOutputVars)
			return false;
		++aIndex; // Convert to one-based.
		for (int i = 0; i < MAX_FUNC_OUTPUT_VAR && mOutputVars[i]; ++i)
			if (mOutputVars[i] == aIndex)
				return true;
		return false;
	}

	// bool result indicates whether aResultToken contains a value (i.e. false for FAIL/EARLY_EXIT).
	bool Call(ResultToken &aResultToken, ExprTokenType *aParam[], int aParamCount, bool aIsVariadic = false);
	bool Call(ResultToken &aResultToken, int aParamCount, ...);

// Macros for specifying arguments in Func::Call(aResultToken, aParamCount, ...)
#define FUNC_ARG_INT(_arg)   SYM_INTEGER, static_cast<__int64>(_arg)
#define FUNC_ARG_STR(_arg)   SYM_STRING,  static_cast<LPTSTR>(_arg)
#define FUNC_ARG_FLOAT(_arg) SYM_FLOAT,   static_cast<double>(_arg)
#define FUNC_ARG_OBJ(_arg)   SYM_OBJECT,  static_cast<IObject *>(_arg)

	ResultType Call(ResultToken *aResultToken)
	{
		// Launch the function similar to Gosub (i.e. not as a new quasi-thread):
		// The performance gain of conditionally passing NULL in place of result (when this is the
		// outermost function call of a line consisting only of function calls, namely ACT_EXPRESSION)
		// would not be significant because the Return command's expression (arg1) must still be evaluated
		// in case it calls any functions that have side-effects, e.g. "return LogThisError()".
		Func *prev_func = g->CurrentFunc; // This will be non-NULL when a function is called from inside another function.
		g->CurrentFunc = this;
		// Although a GOTO that jumps to a position outside of the function's body could be supported,
		// it seems best not to for these reasons:
		// 1) The extreme rarity of a legitimate desire to intentionally do so.
		// 2) The fact that any return encountered after the Goto cannot provide a return value for
		//    the function because load-time validation checks for this (it's preferable not to
		//    give up this check, since it is an informative error message and might also help catch
		//    bugs in the script).  Gosub does not suffer from this because the return that brings it
		//    back into the function body belongs to the Gosub and not the function itself.
		// 3) More difficult to maintain because we have handle jump_to_line the same way ExecUntil() does,
		//    checking aResult the same way it does, then checking jump_to_line the same way it does, etc.
		// Fix for v1.0.31.05: g->mLoopFile and the other g_script members that follow it are
		// now passed to ExecUntil() for two reasons (update for v1.0.44.14: now they're implicitly "passed"
		// because they're done via parameter anymore):
		// 1) To fix the fact that any function call in one parameter of a command would reset
		// A_Index and related variables so that if those variables are referenced in another
		// parameter of the same command, they would be wrong.
		// 2) So that the caller's value of A_Index and such will always be valid even inside
		// of called functions (unless overridden/eclipsed by a loop in the body of the function),
		// which seems to add flexibility without giving up anything.  This fix is necessary at least
		// for a command that references A_Index in two of its args such as the following:
		// ToolTip, O, ((cos(A_Index) * 500) + 500), A_Index
		++mInstances;

		ResultType result;
		DEBUGGER_STACK_PUSH(this)
		result = mJumpToLine->ExecUntil(UNTIL_BLOCK_END, aResultToken);
#ifdef CONFIG_DEBUGGER
		if (g_Debugger.IsConnected())
		{
			if (result == EARLY_RETURN)
			{
				// Find the end of this function.
				//Line *line;
				//for (line = mJumpToLine; line && (line->mActionType != ACT_BLOCK_END || !line->mAttribute); line = line->mNextLine);
				// Since mJumpToLine points at the first line *inside* the body, mJumpToLine->mPrevLine
				// is the block-begin.  That line's mRelatedLine is the line *after* the block-end, so
				// use it's mPrevLine.  mRelatedLine is guaranteed to be non-NULL by load-time logic.
				Line *line = mJumpToLine->mPrevLine->mRelatedLine->mPrevLine;
				// Give user the opportunity to inspect variables before returning.
				if (line)
					g_Debugger.PreExecLine(line);
			}
		}
#endif
		DEBUGGER_STACK_POP()

		--mInstances;
		// Restore the original value in case this function is called from inside another function.
		// Due to the synchronous nature of recursion and recursion-collapse, this should keep
		// g->CurrentFunc accurate, even amidst the asynchronous saving and restoring of "g" itself:
		g->CurrentFunc = prev_func;
		return result;
	}

	// IObject.
	ResultType STDMETHODCALLTYPE Invoke(ResultToken &aResultToken, ExprTokenType &aThisToken, int aFlags, ExprTokenType *aParam[], int aParamCount);
	ULONG STDMETHODCALLTYPE AddRef() { return 1; }
	ULONG STDMETHODCALLTYPE Release() { return 1; }
#ifdef CONFIG_DEBUGGER
	void DebugWriteProperty(IDebugProperties *, int aPage, int aPageSize, int aDepth);
#endif

	Func(LPTSTR aFuncName, bool aIsBuiltIn) // Constructor.
		: mName(aFuncName) // Caller gave us a pointer to dynamic memory for this.
		, mBIF(NULL) // Also initializes mJumpToLine via union.
		, mParam(NULL), mParamCount(0), mMinParams(0) // Also initializes mOutputVar via union (mParam).
		, mFirstLabel(NULL), mLastLabel(NULL)
		, mClass(NULL) // Also initializes mID via union.
		, mVar(NULL), mVarCount(0), mVarCountMax(0), mLazyVar(NULL), mLazyVarCount(0)
		, mStaticVar(NULL), mStaticVarCount(0), mStaticVarCountMax(0), mStaticLazyVar(NULL), mStaticLazyVarCount(0)
		, mGlobalVar(NULL), mGlobalVarCount(0)
		, mInstances(0)
		, mDefaultVarType(VAR_DECLARE_NONE)
		, mIsBuiltIn(aIsBuiltIn)
		, mIsVariadic(false)
		, mHasReturn(false)
	{}
	void *operator new(size_t aBytes) {return SimpleHeap::Malloc(aBytes);}
	void *operator new[](size_t aBytes) {return SimpleHeap::Malloc(aBytes);}
	void operator delete(void *aPtr) {}
	void operator delete[](void *aPtr) {}
};


class ExprOpFunc : public Func
{	// ExprOpFunc: Used in combination with SYM_FUNC to implement certain operations in expressions.
	// These are not inserted into the script's function list, so mName is used only by the debugger.
public:
	ExprOpFunc(BuiltInFunctionType aBIF, int aID)
		: Func(_T("<object>"), true)
	{
		mBIF = aBIF;
		mID = (BuiltInFunctionID)aID;
		// Allow any number of parameters, since these functions aren't called directly by users
		// and might break the rules in some cases, such as Op_ObjGetInPlace() having 0 *visible*
		// parameters but actually reading 2 which are then also passed to the next function call.
		mParamCount = 1000;
	}
};


struct FuncEntry
{
	LPCTSTR mName;
	BuiltInFunctionType mBIF;
	UCHAR mMinParams, mMaxParams;
	bool mHasReturn;
	UCHAR mID;
	UCHAR mOutputVars[MAX_FUNC_OUTPUT_VAR];
};



class ScriptTimer
{
public:
	LabelRef mLabel;
	DWORD mPeriod; // v1.0.36.33: Changed from int to DWORD to double its capacity.
	DWORD mTimeLastRun;  // TickCount
	int mPriority;  // Thread priority relative to other threads, default 0.
	UCHAR mExistingThreads;  // Whether this timer is already running its subroutine.
	bool mEnabled;
	bool mRunOnlyOnce;
	ScriptTimer *mNextTimer;  // Next items in linked list
	void ScriptTimer::Disable();
	ScriptTimer(IObject *aLabel)
		#define DEFAULT_TIMER_PERIOD 250
		: mLabel(aLabel), mPeriod(DEFAULT_TIMER_PERIOD), mPriority(0) // Default is always 0.
		, mExistingThreads(0), mTimeLastRun(0)
		, mEnabled(false), mRunOnlyOnce(false), mNextTimer(NULL)  // Note that mEnabled must default to false for the counts to be right.
	{}
};



struct MsgMonitorStruct
{
	IObject *func;
	HWND hwnd;
	UINT msg;
	// Keep any members smaller than 4 bytes adjacent to save memory:
	static const UCHAR MAX_INSTANCES = MAX_THREADS_LIMIT; // For maintainability.  Causes a compiler warning if MAX_THREADS_LIMIT > MAX_UCHAR.
	UCHAR instance_count; // Distinct from func.mInstances because the script might have called the function explicitly.
	UCHAR max_instances; // v1.0.47: Support more than one thread.
};


struct MsgMonitorInstance;
class MsgMonitorList
{
	MsgMonitorStruct *mMonitor;
	MsgMonitorInstance *mTop;
	int mCount, mCountMax;

	friend struct MsgMonitorInstance;

public:
	MsgMonitorStruct *Find(UINT aMsg, HWND aHwnd, IObject *aCallback);
	MsgMonitorStruct *Add(UINT aMsg, HWND aHwnd, IObject *aCallback, bool aAppend = TRUE);
	void Remove(MsgMonitorStruct *aMonitor);
#ifdef _USRDLL
	void RemoveAll();
	void Free();
#endif
	ResultType Call(ExprTokenType *aParamValue, int aParamCount, int aInitNewThreadIndex); // Used for OnExit and OnClipboardChange, but not OnMessage.

	MsgMonitorStruct& operator[] (const int aIndex) { return mMonitor[aIndex]; }
	int Count() { return mCount; }

	MsgMonitorList() : mCount(0), mCountMax(0), mMonitor(NULL) {}
};


struct MsgMonitorInstance
{
	MsgMonitorList &list;
	MsgMonitorInstance *previous;
	int index;
	int count;

	MsgMonitorInstance(MsgMonitorList &aList)
		: list(aList), previous(aList.mTop), index(0), count(aList.mCount)
	{
		aList.mTop = this;
	}

	~MsgMonitorInstance()
	{
		list.mTop = previous;
	}
};


#ifndef MINIDLL
#define MAX_MENU_NAME_LENGTH MAX_PATH // For both menu and menu item names.
class UserMenuItem;  // Forward declaration since classes use each other (i.e. a menu *item* can have a submenu).
class UserMenu
{
public:
	LPTSTR mName;  // Dynamically allocated.
	UserMenuItem *mFirstMenuItem, *mLastMenuItem, *mDefault;
	// Keep any fields that aren't an even multiple of 4 adjacent to each other.  This conserves memory
	// due to byte-alignment:
	bool mIncludeStandardItems;
	int mClickCount; // How many clicks it takes to trigger the default menu item.  2 = double-click
	UINT mMenuItemCount;  // The count of user-defined menu items (doesn't include the standard items, if present).
	UserMenu *mNextMenu;  // Next item in linked list
	HMENU mMenu;
	MenuTypeType mMenuType; // MENU_TYPE_POPUP (via CreatePopupMenu) vs. MENU_TYPE_BAR (via CreateMenu).
	HBRUSH mBrush;   // Background color to apply to menu.
	COLORREF mColor; // The color that corresponds to the above.

	// Don't overload new and delete operators in this case since we want to use real dynamic memory
	// (since menus can be read in from a file, destroyed and recreated, over and over).

	UserMenu(LPTSTR aName) // Constructor
		: mName(aName), mFirstMenuItem(NULL), mLastMenuItem(NULL), mDefault(NULL)
		, mIncludeStandardItems(false), mClickCount(2), mMenuItemCount(0), mNextMenu(NULL), mMenu(NULL)
		, mMenuType(MENU_TYPE_POPUP) // The MENU_TYPE_NONE flag is not used in this context.  Default = POPUP.
		, mBrush(NULL), mColor(CLR_DEFAULT)
	{
	}

	ResultType AddItem(LPTSTR aName, UINT aMenuID, IObject *aLabel, UserMenu *aSubmenu, LPTSTR aOptions);
	ResultType DeleteItem(UserMenuItem *aMenuItem, UserMenuItem *aMenuItemPrev);
	ResultType DeleteAllItems();
	ResultType ModifyItem(UserMenuItem *aMenuItem, IObject *aLabel, UserMenu *aSubmenu, LPTSTR aOptions);
	void UpdateOptions(UserMenuItem *aMenuItem, LPTSTR aOptions);
	ResultType RenameItem(UserMenuItem *aMenuItem, LPTSTR aNewName);
	ResultType UpdateName(UserMenuItem *aMenuItem, LPTSTR aNewName);
	ResultType CheckItem(UserMenuItem *aMenuItem);
	ResultType UncheckItem(UserMenuItem *aMenuItem);
	ResultType ToggleCheckItem(UserMenuItem *aMenuItem);
	ResultType EnableItem(UserMenuItem *aMenuItem);
	ResultType DisableItem(UserMenuItem *aMenuItem);
	ResultType ToggleEnableItem(UserMenuItem *aMenuItem);
	ResultType SetDefault(UserMenuItem *aMenuItem = NULL);
	ResultType IncludeStandardItems();
	ResultType ExcludeStandardItems();
	ResultType Create(MenuTypeType aMenuType = MENU_TYPE_NONE); // NONE means UNSPECIFIED in this context.
	void SetColor(LPTSTR aColorName, bool aApplyToSubmenus);
	void ApplyColor(bool aApplyToSubmenus);
	ResultType AppendStandardItems();
	ResultType Destroy();
	ResultType Display(bool aForceToForeground = true, int aX = COORD_UNSPECIFIED, int aY = COORD_UNSPECIFIED);
	UINT GetSubmenuPos(HMENU ahMenu);
	UINT GetItemPos(LPTSTR aMenuItemName);
	bool ContainsMenu(UserMenu *aMenu);
	void UpdateAccelerators();
	// L17: Functions for menu icons.
	ResultType SetItemIcon(UserMenuItem *aMenuItem, LPTSTR aFilename, int aIconNumber, int aWidth);
	ResultType ApplyItemIcon(UserMenuItem *aMenuItem);
	ResultType RemoveItemIcon(UserMenuItem *aMenuItem);
	static BOOL OwnerMeasureItem(LPMEASUREITEMSTRUCT aParam);
	static BOOL OwnerDrawItem(LPDRAWITEMSTRUCT aParam);
};



class UserMenuItem
{
public:
	LPTSTR mName;  // Dynamically allocated.
	size_t mNameCapacity;
	UINT mMenuID;
	LabelRef mLabel;
	UserMenu *mSubmenu;
	UserMenu *mMenu;  // The menu to which this item belongs.  Needed to support script var A_ThisMenu.
	int mPriority;
	// Keep any fields that aren't an even multiple of 4 adjacent to each other.  This conserves memory
	// due to byte-alignment:
	bool mEnabled, mChecked;
	UserMenuItem *mNextMenuItem;  // Next item in linked list
	
	union
	{
		// L17: Implementation of menu item icons is OS-dependent (g_os.IsWinVistaOrLater()).
		
		// Older versions of Windows do not support alpha channels in menu item bitmaps, so owner-drawing
		// must be used for icons with transparent backgrounds to appear correctly. Owner-drawing also
		// prevents the icon colours from inverting when the item is selected. DrawIcon() gives the best
		// results, so we store the icon handle as is.
		//
		HICON mIcon;
		
		// Windows Vista and later support alpha channels via 32-bit bitmaps. Since owner-drawing prevents
		// visual styles being applied to menus, we convert each icon to a 32-bit bitmap, calculating the
		// alpha channel as necessary. This is done only once, when the icon is initially set.
		//
		HBITMAP mBitmap;
	};

	// Constructor:
	UserMenuItem(LPTSTR aName, size_t aNameCapacity, UINT aMenuID, IObject *aLabel, UserMenu *aSubmenu, UserMenu *aMenu);

	// Don't overload new and delete operators in this case since we want to use real dynamic memory
	// (since menus can be read in from a file, destroyed and recreated, over and over).
};



struct FontType
{
	#define MAX_FONT_NAME_LENGTH 63  // Longest name I've seen is 29 chars, "Franklin Gothic Medium Italic". Anyway, there's protection against overflow.
	TCHAR name[MAX_FONT_NAME_LENGTH + 1];
	// Keep any fields that aren't an even multiple of 4 adjacent to each other.  This conserves memory
	// due to byte-alignment:
	bool italic;
	bool underline;
	bool strikeout;
	int point_size; // Decided to use int vs. float to simplify the code in many places. Fractional sizes seem rarely needed.
	int weight;
	DWORD quality; // L19: Allow control over font quality (anti-aliasing, etc.).
	HFONT hfont;
};

#endif

#define	LV_REMOTE_BUF_SIZE 1024  // 8192 (below) seems too large in hindsight, given that an LV can only display the first 260 chars in a field.
#define LV_TEXT_BUF_SIZE 8192  // Max amount of text in a ListView sub-item.  Somewhat arbitrary: not sure what the real limit is, if any.

#ifndef MINIDLL
enum LVColTypes {LV_COL_TEXT, LV_COL_INTEGER, LV_COL_FLOAT}; // LV_COL_TEXT must be zero so that it's the default with ZeroMemory.
struct lv_col_type
{
	UCHAR type;             // UCHAR vs. enum LVColTypes to save memory.
	bool sort_disabled;     // If true, clicking the column will have no automatic sorting effect.
	UCHAR case_sensitive;   // Ignored if type isn't LV_COL_TEXT.  SCS_INSENSITIVE is the default.
	bool unidirectional;    // Sorting cannot be reversed/toggled.
	bool prefer_descending; // Whether this column defaults to descending order (on first click or for unidirectional).
};

struct lv_attrib_type
{
	int sorted_by_col; // Index of column by which the control is currently sorted (-1 if none).
	bool is_now_sorted_ascending; // The direction in which the above column is currently sorted.
	bool no_auto_sort; // Automatic sorting disabled.
	#define LV_MAX_COLUMNS 200
	lv_col_type col[LV_MAX_COLUMNS];
	int col_count; // Number of columns currently in the above array.
	int row_count_hint;
};

typedef UCHAR TabControlIndexType;
typedef UCHAR TabIndexType;
// Keep the below in sync with the size of the types above:
#define MAX_TAB_CONTROLS 255  // i.e. the value 255 itself is reserved to mean "doesn't belong to a tab".
#define MAX_TABS_PER_CONTROL 256
struct GuiControlType
{
	HWND hwnd;
	// Keep any fields that are smaller than 4 bytes adjacent to each other.  This conserves memory
	// due to byte-alignment.  It has been verified to save 4 bytes per struct in this case:
	GuiControls type;
	#define GUI_CONTROL_ATTRIB_IMPLICIT_CANCEL     0x01
	#define GUI_CONTROL_ATTRIB_ALTSUBMIT           0x02
	#define GUI_CONTROL_ATTRIB_LABEL_IS_RUNNING    0x04
	#define GUI_CONTROL_ATTRIB_EXPLICITLY_HIDDEN   0x08
	#define GUI_CONTROL_ATTRIB_EXPLICITLY_DISABLED 0x10
	#define GUI_CONTROL_ATTRIB_BACKGROUND_DEFAULT  0x20 // i.e. Don't conform to window/control background color; use default instead.
	#define GUI_CONTROL_ATTRIB_BACKGROUND_TRANS    0x40 // i.e. Leave this control's background transparent.
	#define GUI_CONTROL_ATTRIB_ALTBEHAVIOR         0x80 // For sliders: Reverse/Invert the value. Also for up-down controls (ALT means 32-bit vs. 16-bit). Also for ListView and Tab, and for Edit.
	UCHAR attrib; // A field of option flags/bits defined above.
	TabControlIndexType tab_control_index; // Which tab control this control belongs to, if any.
	TabIndexType tab_index; // For type==TAB, this stores the tab control's index.  For other types, it stores the page.
	Var *output_var;
	LabelRef jump_to_label;
	union
	{
		COLORREF union_color;  // Color of the control's text.
		HBITMAP union_hbitmap; // For PIC controls, stores the bitmap.
		// Note: Pic controls cannot obey the text color, but they can obey the window's background
		// color if the picture's background is transparent (at least in the case of icons on XP).
		lv_attrib_type *union_lv_attrib; // For ListView: Some attributes and an array of columns.
	};
	int mX, mY, mWidth, mHeight;
	float mAX, mAY, mAWidth, mAHeight;
	bool mAXReset, mAYReset, mAXAuto, mAYAuto, mAWAuto, mAHAuto;
	#define USES_FONT_AND_TEXT_COLOR(type) !(type == GUI_CONTROL_PIC || type == GUI_CONTROL_UPDOWN \
		|| type == GUI_CONTROL_SLIDER || type == GUI_CONTROL_PROGRESS)
};

struct GuiControlOptionsType
{
	DWORD style_add, style_remove, exstyle_add, exstyle_remove, listview_style;
	int listview_view; // Viewing mode, such as LVS_ICON, LVS_REPORT.  Int vs. DWORD to more easily use any negative value as "invalid".
	HIMAGELIST himagelist;
	Var *hwnd_output_var; // v1.0.46.01: Allows a script to retrieve the control's HWND upon creation of control.
	int x, y, width, height;  // Position info.
	float row_count;
	int choice;  // Which item of a DropDownList/ComboBox/ListBox to initially choose.
	int range_min, range_max;  // Allowable range, such as for a slider control.
	int tick_interval; // The interval at which to draw tickmarks for a slider control.
	int line_size, page_size; // Also for slider.
	int thickness;  // Thickness of slider's thumb.
	int tip_side; // Which side of the control to display the tip on (0 to use default side).
	GuiControlType *buddy1, *buddy2;
	COLORREF color_listview; // Used only for those controls that need control.union_color for something other than color.
	COLORREF color_bk; // Control's background color.
	int limit;   // The max number of characters to permit in an edit or combobox's edit (also used by ListView).
	int hscroll_pixels;  // The number of pixels for a listbox's horizontal scrollbar to be able to scroll.
	int checked; // When zeroed, struct contains default starting state of checkbox/radio, i.e. BST_UNCHECKED.
	int icon_number; // Which icon of a multi-icon file to use.  Zero means use-default, i.e. the first icon.
	#define GUI_MAX_TABSTOPS 50
	UINT tabstop[GUI_MAX_TABSTOPS]; // Array of tabstops for the interior of a multi-line edit control.
	UINT tabstop_count;  // The number of entries in the above array.
	SYSTEMTIME sys_time[2]; // Needs to support 2 elements for MONTHCAL's multi/range mode.
	SYSTEMTIME sys_time_range[2];
	DWORD gdtr, gdtr_range; // Used in connection with sys_time and sys_time_range.
	ResultType redraw;  // Whether the state of WM_REDRAW should be changed.
	TCHAR password_char; // When zeroed, indicates "use default password" for an edit control with the password style.
	bool range_changed;
	bool color_changed; // To discern when a control has been put back to the default color. [v1.0.26]
	bool start_new_section;
	bool use_theme; // v1.0.32: Provides the means for the window's current setting of mUseTheme to be overridden.
	bool listview_no_auto_sort; // v1.0.44: More maintainable and frees up GUI_CONTROL_ATTRIB_ALTBEHAVIOR for other uses.
	ATOM customClassAtom;
	float AX, AY, AWidth, AHeight;
	bool AXReset, AYReset, AXAuto, AYAuto, AWAuto, AHAuto;
};

LRESULT CALLBACK GuiWindowProc(HWND hWnd, UINT iMsg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK TabWindowProc(HWND hWnd, UINT iMsg, WPARAM wParam, LPARAM lParam);

class GuiType
{
public:
	#define GUI_STANDARD_WIDTH_MULTIPLIER 15 // This times font size = width, if all other means of determining it are exhausted.
	#define GUI_STANDARD_WIDTH DPIScale(GUI_STANDARD_WIDTH_MULTIPLIER * sFont[mCurrentFontIndex].point_size)
	// Update for v1.0.21: Reduced it to 8 vs. 9 because 8 causes the height each edit (with the
	// default style) to exactly match that of a Combo or DropDownList.  This type of spacing seems
	// to be what other apps use too, and seems to make edits stand out a little nicer:
	#define GUI_CTL_VERTICAL_DEADSPACE DPIScale(8)
	#define PROGRESS_DEFAULT_THICKNESS DPIScale(2 * sFont[mCurrentFontIndex].point_size)
	LPTSTR mName;
	HWND mHwnd, mStatusBarHwnd;
	HWND mOwner;  // The window that owns this one, if any.  Note that Windows provides no way to change owners after window creation.
	// Control IDs are higher than their index in the array by the below amount.  This offset is
	// necessary because windows that behave like dialogs automatically return IDOK and IDCANCEL in
	// response to certain types of standard actions:
	GuiIndexType mControlCount;
	GuiIndexType mControlCapacity; // How many controls can fit into the current memory size of mControl.
	GuiControlType *mControl; // Will become an array of controls when the window is first created.
	GuiIndexType mDefaultButtonIndex; // Index vs. pointer is needed for some things.
	ULONG mReferenceCount; // For keeping this structure in memory during execution of the Gui's labels.
	LabelPtr mLabelForClose, mLabelForEscape, mLabelForSize, mLabelForDropFiles, mLabelForContextMenu; // These aren't reference counted, as they can only be a Func or Label, not a dynamic object.
	bool mLabelForCloseIsRunning, mLabelForEscapeIsRunning, mLabelForSizeIsRunning; // DropFiles doesn't need one of these.
	bool mLabelsHaveBeenSet;
	DWORD mStyle, mExStyle; // Style of window.
	bool mInRadioGroup; // Whether the control currently being created is inside a prior radio-group.
	bool mUseTheme;  // Whether XP theme and styles should be applied to the parent window and subsequently added controls.
	TCHAR mDelimiter;  // The default field delimiter when adding items to ListBox, DropDownList, ListView, etc.
	GuiControlType *mCurrentListView, *mCurrentTreeView; // The ListView and TreeView upon which the LV/TV functions operate.
	int mCurrentFontIndex;
	COLORREF mCurrentColor;       // The default color of text in controls.
	COLORREF mBackgroundColorWin; // The window's background color itself.
	COLORREF mBackgroundColorCtl; // Background color for controls.
	HBRUSH mBackgroundBrushWin;   // Brush corresponding to mBackgroundColorWin.
	HBRUSH mBackgroundBrushCtl;   // Brush corresponding to mBackgroundColorCtl.
	HDROP mHdrop;                 // Used for drag and drop operations.
	HICON mIconEligibleForDestruction; // The window's icon, which can be destroyed when the window is destroyed if nothing else is using it.
	HICON mIconEligibleForDestructionSmall; // L17: A window may have two icons: ICON_SMALL and ICON_BIG.
	HACCEL mAccel; // Keyboard accelerator table.
	int mMarginX, mMarginY, mPrevX, mPrevY, mPrevWidth, mPrevHeight, mMaxExtentRight, mMaxExtentDown
		, mSectionX, mSectionY, mMaxExtentRightSection, mMaxExtentDownSection, mWidth, mHeight;
	LONG mMinWidth, mMinHeight, mMaxWidth, mMaxHeight;
	TabControlIndexType mTabControlCount;
	TabControlIndexType mCurrentTabControlIndex; // Which tab control of the window.
	TabIndexType mCurrentTabIndex;// Which tab of a tab control is currently the default for newly added controls.
	bool mGuiShowHasNeverBeenDone, mFirstActivation, mShowIsInProgress, mDestroyWindowHasBeenCalled;
	bool mControlWidthWasSetByContents; // Whether the most recently added control was auto-width'd to fit its contents.
	bool mUsesDPIScaling; // Whether the GUI uses DPI scaling.

	SCROLLINFO *mVScroll, *mHScroll;
	#define MAX_GUI_FONTS 200  // v1.0.44.14: Increased from 100 to 200 due to feedback that 100 wasn't enough.  But to alleviate memory usage, the array is now allocated upon first use.
	static FontType *sFont; // An array of structs, allocated upon first use.
	static int sFontCount;
	static HWND sTreeWithEditInProgress; // Needed because TreeView's edit control for label-editing conflicts with IDOK (default button).

	// Don't overload new and delete operators in this case since we want to use real dynamic memory
	// (since GUIs can be destroyed and recreated, over and over).

	// Keep the default destructor to avoid entering the "Law of the Big Three": If your class requires a
	// copy constructor, copy assignment operator, or a destructor, then it very likely will require all three.

	GuiType() // Constructor
		: mName(NULL), mHwnd(NULL), mStatusBarHwnd(NULL), mControlCount(0), mControlCapacity(0)
		, mDefaultButtonIndex(-1), mLabelForClose(NULL), mLabelForEscape(NULL), mLabelForSize(NULL)
		, mLabelForDropFiles(NULL), mLabelForContextMenu(NULL), mReferenceCount(1)
		, mLabelForCloseIsRunning(false), mLabelForEscapeIsRunning(false), mLabelForSizeIsRunning(false)
		, mLabelsHaveBeenSet(false), mUsesDPIScaling(true), mVScroll(NULL), mHScroll(NULL)
		// The styles DS_CENTER and DS_3DLOOK appear to be ineffectual in this case.
		// Also note that WS_CLIPSIBLINGS winds up on the window even if unspecified, which is a strong hint
		// that it should always be used for top level windows across all OSes.  Usenet posts confirm this.
		// Also, it seems safer to have WS_POPUP under a vague feeling that it seems to apply to dialog
		// style windows such as this one, and the fact that it also allows the window's caption to be
		// removed, which implies that POPUP windows are more flexible than OVERLAPPED windows.
		, mStyle(WS_POPUP|WS_CLIPSIBLINGS|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX) // WS_CLIPCHILDREN (doesn't seem helpful currently)
		, mExStyle(0) // This and the above should not be used once the window has been created since they might get out of date.
		, mInRadioGroup(false), mUseTheme(true), mOwner(NULL), mDelimiter('|')
		, mCurrentFontIndex(FindOrCreateFont()) // Must call this in constructor to ensure sFont array is never NULL while a GUI object exists.  Omit params to tell it to find or create DEFAULT_GUI_FONT.
		, mCurrentListView(NULL), mCurrentTreeView(NULL)
		, mTabControlCount(0), mCurrentTabControlIndex(MAX_TAB_CONTROLS), mCurrentTabIndex(0)
		, mCurrentColor(CLR_DEFAULT)
		, mBackgroundColorWin(CLR_DEFAULT), mBackgroundBrushWin(NULL)
		, mBackgroundColorCtl(CLR_DEFAULT), mBackgroundBrushCtl(NULL)
		, mHdrop(NULL), mIconEligibleForDestruction(NULL), mIconEligibleForDestructionSmall(NULL)
		, mAccel(NULL)
		, mMarginX(COORD_UNSPECIFIED), mMarginY(COORD_UNSPECIFIED) // These will be set when the first control is added.
		, mPrevX(0), mPrevY(0)
		, mPrevWidth(0), mPrevHeight(0) // Needs to be zero for first control to start off at right offset.
		, mMaxExtentRight(0), mMaxExtentDown(0)
		, mSectionX(COORD_UNSPECIFIED), mSectionY(COORD_UNSPECIFIED)
		, mMaxExtentRightSection(COORD_UNSPECIFIED), mMaxExtentDownSection(COORD_UNSPECIFIED)
		, mMinWidth(COORD_UNSPECIFIED), mMinHeight(COORD_UNSPECIFIED)
		, mMaxWidth(COORD_UNSPECIFIED), mMaxHeight(COORD_UNSPECIFIED), mWidth(COORD_UNSPECIFIED), mHeight(COORD_UNSPECIFIED)
		, mGuiShowHasNeverBeenDone(true), mFirstActivation(true), mShowIsInProgress(false)
		, mDestroyWindowHasBeenCalled(false), mControlWidthWasSetByContents(false)
	{
		// The array of controls is left uninitialized to catch bugs.  Each control's attributes should be
		// fully populated when it is created.
		//ZeroMemory(mControl, sizeof(mControl));
	}

	static ResultType Destroy(GuiType &gui);
	static void DestroyIconsIfUnused(HICON ahIcon, HICON ahIconSmall); // L17: Renamed function and added parameter to also handle the window's small icon.
	ResultType Create();
	void AddRef();
	void Release();
	void SetLabels(LPTSTR aLabelPrefix);
	static LPTSTR ConvertEvent(GuiEventType evt);
	static IObject* CreateDropArray(HDROP hDrop);
	static void UpdateMenuBars(HMENU aMenu);
	ResultType AddControl(GuiControls aControlType, LPTSTR aOptions, LPTSTR aText);

	ResultType ParseOptions(LPTSTR aOptions, bool &aSetLastFoundWindow, ToggleValueType &aOwnDialogs, Var *&aHwndVar);
	void GetNonClientArea(LONG &aWidth, LONG &aHeight);
	void GetTotalWidthAndHeight(LONG &aWidth, LONG &aHeight);

	ResultType ControlParseOptions(LPTSTR aOptions, GuiControlOptionsType &aOpt, GuiControlType &aControl
		, GuiIndexType aControlIndex = -1, Var *aParam3Var = NULL); // aControlIndex is not needed upon control creation.
	void ControlInitOptions(GuiControlOptionsType &aOpt, GuiControlType &aControl);
	void ControlAddContents(GuiControlType &aControl, LPTSTR aContent, int aChoice, GuiControlOptionsType *aOpt = NULL);
	ResultType Show(LPTSTR aOptions, LPTSTR aTitle);
	ResultType Clear();
	ResultType Cancel();
	ResultType Close(); // Due to SC_CLOSE, etc.
	ResultType Escape(); // Similar to close, except typically called when the user presses ESCAPE.
	ResultType Submit(bool aHideIt);
	ResultType ControlGetContents(Var &aOutputVar, GuiControlType &aControl, LPTSTR aMode = _T(""));

	static VarSizeType ControlGetName(GuiType *aGuiWindow, GuiIndexType aControlIndex, LPTSTR aBuf);
	
	static GuiType *FindGui(LPTSTR aName);
	static GuiType *FindGui(HWND aHwnd);
	static GuiType *FindGuiParent(HWND aHwnd);

	static GuiType *ValidGui(GuiType *&aGuiRef); // Updates aGuiRef if it points to a destroyed Gui.

	GuiIndexType FindControl(LPTSTR aControlID);
	GuiControlType *FindControl(HWND aHwnd, bool aRetrieveIndexInstead = false)
	{
		GuiIndexType index = GUI_HWND_TO_INDEX(aHwnd); // Retrieves a small negative on failure, which will be out of bounds when converted to unsigned.
		if (index >= mControlCount) // Not found yet; try again with parent.
		{
			// Since ComboBoxes (and possibly other future control types) have children, try looking
			// up aHwnd's parent to see if its a known control of this dialog.  Some callers rely on us making
			// this extra effort:
			if (aHwnd = GetParent(aHwnd)) // Note that a ComboBox's drop-list (class ComboLBox) is apparently a direct child of the desktop, so this won't help us in that case.
				index = GUI_HWND_TO_INDEX(aHwnd); // Retrieves a small negative on failure, which will be out of bounds when converted to unsigned.
		}
		if (index < mControlCount && mControl[index].hwnd == aHwnd) // A match was found.  Fix for v1.1.09.03: Confirm it is actually one of our controls.
			return aRetrieveIndexInstead ? (GuiControlType *)(size_t)index : mControl + index;
		else // No match, so indicate failure.
			return aRetrieveIndexInstead ? (GuiControlType *)NO_CONTROL_INDEX : NULL;
	}
	int FindGroup(GuiIndexType aControlIndex, GuiIndexType &aGroupStart, GuiIndexType &aGroupEnd);

	ResultType SetCurrentFont(LPTSTR aOptions, LPTSTR aFontName);
	static int FindOrCreateFont(LPTSTR aOptions = _T(""), LPTSTR aFontName = _T(""), FontType *aFoundationFont = NULL
		, COLORREF *aColor = NULL);
	static int FindFont(FontType &aFont);

	void Event(GuiIndexType aControlIndex, UINT aNotifyCode, USHORT aGuiEvent = GUI_EVENT_NONE, UINT_PTR aEventInfo = 0);
	LRESULT CustomCtrlWmNotify(GuiIndexType aControlIndex, LPNMHDR aNmHdr);

	static WORD TextToHotkey(LPTSTR aText);
	static LPTSTR HotkeyToText(WORD aHotkey, LPTSTR aBuf);
	void ControlCheckRadioButton(GuiControlType &aControl, GuiIndexType aControlIndex, WPARAM aCheckType);
	void ControlSetUpDownOptions(GuiControlType &aControl, GuiControlOptionsType &aOpt);
	int ControlGetDefaultSliderThickness(DWORD aStyle, int aThumbThickness);
	void ControlSetSliderOptions(GuiControlType &aControl, GuiControlOptionsType &aOpt);
	int ControlInvertSliderIfNeeded(GuiControlType &aControl, int aPosition);
	void ControlSetListViewOptions(GuiControlType &aControl, GuiControlOptionsType &aOpt);
	void ControlSetTreeViewOptions(GuiControlType &aControl, GuiControlOptionsType &aOpt);
	void ControlSetProgressOptions(GuiControlType &aControl, GuiControlOptionsType &aOpt, DWORD aStyle);
	bool ControlOverrideBkColor(GuiControlType &aControl);

	void ControlUpdateCurrentTab(GuiControlType &aTabControl, bool aFocusFirstControl);
	GuiControlType *FindTabControl(TabControlIndexType aTabControlIndex);
	int FindTabIndexByName(GuiControlType &aTabControl, LPTSTR aName, bool aExactMatch = false);
	int GetControlCountOnTabPage(TabControlIndexType aTabControlIndex, TabIndexType aTabIndex);
	POINT GetPositionOfTabClientArea(GuiControlType &aTabControl);
	ResultType SelectAdjacentTab(GuiControlType &aTabControl, bool aMoveToRight, bool aFocusFirstControl
		, bool aWrapAround);
	void ControlGetPosOfFocusedItem(GuiControlType &aControl, POINT &aPoint);
	static void LV_Sort(GuiControlType &aControl, int aColumnIndex, bool aSortOnlyIfEnabled, TCHAR aForceDirection = '\0');
	static DWORD ControlGetListViewMode(HWND aWnd);
	static IObject *ControlGetActiveX(HWND aWnd);
	
	void UpdateAccelerators(UserMenu &aMenu);
	void UpdateAccelerators(UserMenu &aMenu, LPACCEL aAccel, int &aAccelCount);
	void RemoveAccelerators();
	static bool ConvertAccelerator(LPTSTR aString, ACCEL &aAccel);

	// See DPIScale() and DPIUnscale() for more details.
	int Scale(int x) { return mUsesDPIScaling ? DPIScale(x) : x; }
	int Unscale(int x) { return mUsesDPIScaling ? DPIUnscale(x) : x; }
	// The following is a workaround for the "w-1" and "h-1" options:
	int ScaleSize(int x) { return mUsesDPIScaling && x != -1 ? DPIScale(x) : x; }
};
#endif // MINIDLL

typedef NTSTATUS (NTAPI *PFN_NT_QUERY_INFORMATION_PROCESS) (
    HANDLE ProcessHandle,
    PROCESSINFOCLASS ProcessInformationClass,
    PVOID ProcessInformation,
    ULONG ProcessInformationLength,
    PULONG ReturnLength OPTIONAL);


class Script
{
private:
#ifndef MINIDLL
	friend class Hotkey;
#endif
#ifdef CONFIG_DEBUGGER
	friend class Debugger;
#endif

public:	
	Var **mVar, **mLazyVar; // Array of pointers-to-variable, allocated upon first use and later expanded as needed.
	int mVarCount, mVarCountMax, mLazyVarCount; // Count of items in the above array as well as the maximum capacity.
	WinGroup *mFirstGroup, *mLastGroup;  // The first and last variables in the linked list.
	int mCurrentFuncOpenBlockCount; // While loading the script, this is how many blocks are currently open in the current function's body.
	bool mNextLineIsFunctionBody; // Whether the very next line to be added will be the first one of the body.
	bool mNoUpdateLabels;
	
#define MAX_NESTED_CLASSES 5
#define MAX_CLASS_NAME_LENGTH UCHAR_MAX
	int mClassObjectCount;
	Object *mClassObject[MAX_NESTED_CLASSES]; // Class definition currently being parsed.
	TCHAR mClassName[MAX_CLASS_NAME_LENGTH + 1]; // Only used during load-time.
	Object *mUnresolvedClasses;
	Property *mClassProperty;
	LPTSTR mClassPropertyDef;

	// These two track the file number and line number in that file of the line currently being loaded,
	// which simplifies calls to ScriptError() and LineError() (reduces the number of params that must be passed).
	// These are used ONLY while loading the script into memory.  After that (while the script is running),
	// only mCurrLine is kept up-to-date:
	int mCurrFileIndex;
	LineNumberType mCombinedLineNumber; // In the case of a continuation section/line(s), this is always the top line.

	bool mNoHotkeyLabels;
#ifndef MINIDLL
	bool mMenuUseErrorLevel;  // Whether runtime errors should be displayed by the Menu command, vs. ErrorLevel.
#endif

	#define UPDATE_TIP_FIELD tcslcpy(mNIC.szTip, (mTrayIconTip && *mTrayIconTip) ? mTrayIconTip \
		: (mFileName ? mFileName : T_AHK_NAME), _countof(mNIC.szTip));
	NOTIFYICONDATA mNIC; // For ease of adding and deleting our tray icon.

	size_t GetLine(LPTSTR aBuf, int aMaxCharsToRead, int aInContinuationSection, TextStream *ts);
	ResultType IsDirective(LPTSTR aBuf);
	ResultType ParseAndAddLine(LPTSTR aLineText, ActionTypeType aActionType = ACT_INVALID
		, LPTSTR aLiteralMap = NULL, size_t aLiteralMapLength = 0);
	ResultType ParseDerefs(LPTSTR aArgText, LPTSTR aArgMap, DerefType *aDeref, int &aDerefCount, int *aPos = NULL, TCHAR aEndChar = 0);
	ResultType ParseOperands(LPTSTR aArgText, LPTSTR aArgMap, DerefType *aDeref, int &aDerefCount, int *aPos = NULL, TCHAR aEndChar = 0);
	ResultType ParseDoubleDeref(LPTSTR aArgText, LPTSTR aArgMap, DerefType *aDeref, int &aDerefCount, int *aPos);
	LPTSTR ParseActionType(LPTSTR aBufTarget, LPTSTR aBufSource, bool aDisplayErrors);
	static ActionTypeType ConvertActionType(LPTSTR aActionTypeString);
	ResultType AddLabel(LPTSTR aLabelName, bool aAllowDupe);
	ResultType AddLine(ActionTypeType aActionType, LPTSTR aArg[] = NULL, int aArgc = 0, LPTSTR aArgMap[] = NULL);

	// These aren't in the Line class because I think they're easier to implement
	// if aStartingLine is allowed to be NULL (for recursive calls).  If they
	// were member functions of class Line, a check for NULL would have to
	// be done before dereferencing any line's mNextLine, for example:
	ResultType PreparseExpressions(Line *aStartingLine);
	ResultType PreparseStaticLines(Line *aStartingLine);
	Line *PreparseBlocks(Line *aStartingLine, ExecUntilMode aMode = NORMAL_MODE, Line *aParentLine = NULL, const ActionTypeType aLoopType = ACT_INVALID);
	Line *PreparseCommands(Line *aStartingLine);

	Line *mFirstLine, *mLastLine;     // The first and last lines in the linked list.
	Line *mFirstStaticLine, *mLastStaticLine; // The first and last static var initializer.
	Label *mFirstLabel, *mLastLabel;  // The first and last labels in the linked list.
	Func **mFunc;  // Binary-searchable array of functions.
	int mFuncCount, mFuncCountMax;
	Line *mTempLine; // for use with dll Execute # Naveen N9
	Label *mTempLabel; // for use with dll Execute # Naveen N9
	Func *mTempFunc; // for use with dll Execute # Naveen N9

	// Naveen moved above from private
	Line *mCurrLine;     // Seems better to make this public than make Line our friend.
	Label *mPlaceholderLabel; // Used in place of a NULL label to simplify code.
#ifndef MINIDLL
	TCHAR mThisMenuItemName[MAX_MENU_NAME_LENGTH + 1];
	TCHAR mThisMenuName[MAX_MENU_NAME_LENGTH + 1];
	LPTSTR mThisHotkeyName, mPriorHotkeyName;
#endif
	MsgMonitorList mOnExit, mOnClipboardChange; // Lists of event handlers for OnExit() and OnClipboardChange().
	HWND mNextClipboardViewer;
	bool mOnClipboardChangeIsRunning;
	ExitReasons mExitReason;

	ScriptTimer *mFirstTimer, *mLastTimer;  // The first and last script timers in the linked list.
	UINT mTimerCount, mTimerEnabledCount;
#ifndef MINIDLL
	UserMenu *mFirstMenu, *mLastMenu;
	UINT mMenuCount;
#endif
	DWORD mThisHotkeyStartTime, mPriorHotkeyStartTime;  // Tickcount timestamp of when its subroutine began.
#ifndef MINIDLL
	TCHAR mEndChar;  // The ending character pressed to trigger the most recent non-auto-replace hotstring.
#endif
	modLR_type mThisHotkeyModifiersLR;
	LPTSTR mFileSpec; // Will hold the full filespec, for convenience.
	LPTSTR mFileDir;  // Will hold the directory containing the script file.
	LPTSTR mFileName; // Will hold the script's naked file name.
	LPTSTR mOurEXE; // Will hold this app's module name (e.g. C:\Program Files\AutoHotkey\AutoHotkey.exe).
	LPTSTR mOurEXEDir;  // Same as above but just the containing directory (for convenience).
	LPTSTR mMainWindowTitle; // Will hold our main window's title, for consistency & convenience.
	bool mIsReadyToExecute;
	bool mAutoExecSectionIsRunning;
	bool mIsRestart; // The app is restarting rather than starting from scratch.
	bool mErrorStdOut; // true if load-time syntax errors should be sent to stdout vs. a MsgBox.
#ifndef AUTOHOTKEYSC
	TextStream *mIncludeLibraryFunctionsThenExit;
#endif
	int mUninterruptedLineCountMax; // 32-bit for performance (since huge values seem unnecessary here).
	int mUninterruptibleTime;
	DWORD mLastPeekTime;

	CStringW mRunAsUser, mRunAsPass, mRunAsDomain;
#ifndef MINIDLL

	HICON mCustomIcon;  // NULL unless the script has loaded a custom icon during its runtime.
	HICON mCustomIconSmall; // L17: Use separate big/small icons for best results.
	LPTSTR mCustomIconFile; // Filename of icon.  Allocated on first use.
	bool mIconFrozen; // If true, the icon does not change state when the state of pause or suspend changes.
	LPTSTR mTrayIconTip;  // Custom tip text for tray icon.  Allocated on first use.
	UINT mCustomIconNumber; // The number of the icon inside the above file.

	UserMenu *mTrayMenu; // Our tray menu, which should be destroyed upon exiting the program.
#endif
	ResultType Init(global_struct &g, LPTSTR aScriptFilename, bool aIsRestart, HINSTANCE hInstance,bool aIsText);
	// ResultType InitDll(global_struct &g,HINSTANCE hInstance); // HotKeyIt init dll from text
	ResultType CreateWindows();
	void EnableClipboardListener(bool aEnable);
#ifndef MINIDLL
	void EnableOrDisableViewMenuItems(HMENU aMenu, UINT aFlags);
	void CreateTrayIcon();
	void UpdateTrayIcon(bool aForceUpdate = false);
#endif
	ResultType AutoExecSection();
#ifndef MINIDLL
	ResultType Edit();
#endif
	bool IsPersistent();
	void ExitIfNotPersistent(ExitReasons aExitReason);
	ResultType Reload(bool aDisplayErrors);
	ResultType ExitApp(ExitReasons aExitReason, LPTSTR aBuf = NULL, int ExitCode = 0);
	void TerminateApp(ExitReasons aExitReason, int aExitCode); // L31: Added aExitReason. See script.cpp.
	LineNumberType LoadFromFile();
#ifndef AUTOHOTKEYSC
	LineNumberType LoadFromText(LPTSTR aScript,LPCTSTR aPathToShow = NULL); // HotKeyIt H1 load text instead file ahktextdll
	ResultType LoadIncludedText(LPTSTR aScript,LPCTSTR aPathToShow = NULL); //New read text
#endif
	ResultType LoadIncludedFile(LPTSTR aFileSpec, bool aAllowDuplicateInclude, bool aIgnoreLoadFailure);
	ResultType UpdateOrCreateTimer(IObject *aLabel, LPTSTR aPeriod, LPTSTR aPriority, bool aEnable
		, bool aUpdatePriorityOnly);
	void DeleteTimer(IObject *aLabel);

	ResultType DefineFunc(LPTSTR aBuf, Var *aFuncGlobalVar[]);
#ifndef AUTOHOTKEYSC
	Func *FindFuncInLibrary(LPTSTR aFuncName, size_t aFuncNameLength, bool &aErrorWasShown, bool &aFileWasFound, bool aIsAutoInclude);
#endif
	Func *FindFunc(LPCTSTR aFuncName, size_t aFuncNameLength = 0, int *apInsertPos = NULL);
	Func *AddFunc(LPCTSTR aFuncName, size_t aFuncNameLength, bool aIsBuiltIn, int aInsertPos, Object *aClassObject = NULL);

	ResultType DefineClass(LPTSTR aBuf);
	ResultType DefineClassVars(LPTSTR aBuf, bool aStatic);
	ResultType DefineClassProperty(LPTSTR aBuf);
	Object *FindClass(LPCTSTR aClassName, size_t aClassNameLength = 0);
	ResultType ResolveClasses();

	int AddBIF(LPTSTR aFuncName, BuiltInFunctionType bif, size_t minparams, size_t maxparams); // N10 added for dynamic BIFs
	#define FINDVAR_DEFAULT  (VAR_LOCAL | VAR_GLOBAL)
	#define FINDVAR_GLOBAL   VAR_GLOBAL
	#define FINDVAR_LOCAL    VAR_LOCAL
	#define FINDVAR_STATIC    (VAR_LOCAL | VAR_LOCAL_STATIC)
	Var *FindOrAddVar(LPTSTR aVarName, size_t aVarNameLength = 0, int aScope = FINDVAR_DEFAULT);
	Var *FindVar(LPTSTR aVarName, size_t aVarNameLength = 0, int *apInsertPos = NULL
		, int aScope = FINDVAR_DEFAULT
		, bool *apIsLocal = NULL);
	Var *AddVar(LPTSTR aVarName, size_t aVarNameLength, int aInsertPos, int aScope);
	static VarTypes GetVarType(LPTSTR aVarName, VirtualVar *aBIV = NULL);
	static BuiltInVarType GetVarType_BIV(LPTSTR aVarName, BuiltInVarSetType &setter); // Helper function.

	WinGroup *FindGroup(LPTSTR aGroupName, bool aCreateIfNotFound = false);
	ResultType AddGroup(LPTSTR aGroupName);
	Label *FindLabel(LPTSTR aLabelName);
	IObject *FindCallable(LPTSTR aLabelName, Var *aVar = NULL, int aParamCount = 0);

	ResultType DoRunAs(LPTSTR aCommandLine, LPTSTR aWorkingDir, bool aDisplayErrors, bool aUpdateLastError, WORD aShowWindow
		, Var *aOutputVar, PROCESS_INFORMATION &aPI, bool &aSuccess, HANDLE &aNewProcess, LPTSTR aSystemErrorText);
	ResultType ActionExec(LPTSTR aAction, LPTSTR aParams = NULL, LPTSTR aWorkingDir = NULL
		, bool aDisplayErrors = true, LPTSTR aRunShowMode = NULL, HANDLE *aProcess = NULL
		, bool aUpdateLastError = false, bool aUseRunAs = false, Var *aOutputVar = NULL);
#ifndef MINIDLL
	LPTSTR ListVars(LPTSTR aBuf, int aBufSize);
	LPTSTR ListKeyHistory(LPTSTR aBuf, int aBufSize);

	ResultType PerformMenu(LPTSTR aMenu, LPTSTR aCommand, LPTSTR aParam3, LPTSTR aParam4, LPTSTR aOptions, LPTSTR aOptions2, Var *aParam4Var); // L17: Added aOptions2 for Icon sub-command (icon width). Arg was previously reserved/unused.
	UserMenu *FindMenu(LPTSTR aMenuName);
	UserMenu *AddMenu(LPTSTR aMenuName);
	UINT ThisMenuItemPos();
	ResultType ScriptDeleteMenu(UserMenu *aMenu);
	UserMenuItem *FindMenuItemByID(UINT aID)
	{
		UserMenuItem *mi;
		for (UserMenu *m = mFirstMenu; m; m = m->mNextMenu)
			for (mi = m->mFirstMenuItem; mi; mi = mi->mNextMenuItem)
				if (mi->mMenuID == aID)
					return mi;
		return NULL;
	}
	UserMenuItem *FindMenuItemBySubmenu(HMENU aSubmenu) // L26: Used by WM_MEASUREITEM/WM_DRAWITEM to find the menu item with an associated submenu. Fixes icons on such items when owner-drawn menus are in use.
	{
		UserMenuItem *mi;
		for (UserMenu *m = mFirstMenu; m; m = m->mNextMenu)
			for (mi = m->mFirstMenuItem; mi; mi = mi->mNextMenuItem)
				if (mi->mSubmenu && mi->mSubmenu->mMenu == aSubmenu)
					return mi;
		return NULL;
	}

	ResultType PerformGui(LPTSTR aBuf, LPTSTR aControlType, LPTSTR aOptions, LPTSTR aParam4);
	static GuiType *ResolveGui(LPTSTR aBuf, LPTSTR &aCommand, LPTSTR *aName = NULL, size_t *aNameLength = NULL, LPTSTR aControlID = NULL);
#endif
	// Call this SciptError to avoid confusion with Line's error-displaying functions:
	ResultType ScriptError(LPCTSTR aErrorText, LPCTSTR aExtraInfo = _T("")); // , ResultType aErrorType = FAIL);
	void ScriptWarning(WarnMode warnMode, LPCTSTR aWarningText, LPCTSTR aExtraInfo = _T(""), Line *line = NULL);
	void WarnUninitializedVar(Var *var);
	void MaybeWarnLocalSameAsGlobal(Func &func, Var &var);

	void PreprocessLocalVars(Func &aFunc, Var **aVarList, int &aVarCount);

	static ResultType UnhandledException(ResultToken*& aToken, Line* aLine);
	static ResultType SetErrorLevelOrThrow() { return SetErrorLevelOrThrowBool(true); }
	static ResultType SetErrorLevelOrThrowBool(bool aError);
	static ResultType SetErrorLevelOrThrowInt(int aErrorValue, LPCTSTR aWhat = NULL);
	static ResultType SetErrorLevelOrThrowStr(LPCTSTR aErrorValue, LPCTSTR aWhat = NULL);
	static ResultType ThrowRuntimeException(LPCTSTR aErrorText, LPCTSTR aWhat = NULL, LPCTSTR aExtraInfo = _T(""));
	static void FreeExceptionToken(ResultToken*& aToken);

	#define SOUNDPLAY_ALIAS _T("AHK_PlayMe")  // Used by destructor and SoundPlay().

	Script();
	~Script();
	// Note that the anchors to any linked lists will be lost when this
	// object goes away, so for now, be sure the destructor is only called
	// when the program is about to be exited, which will thereby reclaim
	// the memory used by the abandoned linked lists (otherwise, a memory
	// leak will result).
};

#ifdef _USRDLL
class MallocHeap; // forward declaration for export.cpp
#endif

////////////////////////
// BUILT-IN VARIABLES //
////////////////////////

// Declare built-in var read function.
#define BIV_DECL_R(name) VarSizeType name(LPTSTR aBuf, LPTSTR aVarName)
// Declare built-in var write function.
#define BIV_DECL_W(name) ResultType name(LPTSTR aBuf, LPTSTR aVarName)
// Declare built-in var read and write functions.
#define BIV_DECL_RW(name) BIV_DECL_R(name); BIV_DECL_W(name##_Set)

BIV_DECL_R (BIV_True_False_Null);
BIV_DECL_R (BIV_MMM_DDD);
BIV_DECL_R (BIV_DateTime);
BIV_DECL_RW(BIV_TitleMatchMode);
BIV_DECL_RW(BIV_TitleMatchModeSpeed);
BIV_DECL_RW(BIV_DetectHiddenWindows);
BIV_DECL_RW(BIV_DetectHiddenText);
BIV_DECL_RW(BIV_StringCaseSense);
BIV_DECL_RW(BIV_KeyDelay);
BIV_DECL_RW(BIV_WinDelay);
BIV_DECL_RW(BIV_ControlDelay);
BIV_DECL_RW(BIV_MouseDelay);
BIV_DECL_RW(BIV_DefaultMouseSpeed);
BIV_DECL_R (BIV_IsPaused);
BIV_DECL_R (BIV_IsCritical);
#ifndef MINIDLL
BIV_DECL_R (BIV_IsSuspended);
#endif
BIV_DECL_R (BIV_IsCompiled);
BIV_DECL_R (BIV_IsUnicode);
BIV_DECL_RW(BIV_FileEncoding);
BIV_DECL_R (BIV_MsgBoxResult);
BIV_DECL_RW(BIV_RegView);
BIV_DECL_RW(BIV_LastError);
BIV_DECL_R (BIV_GlobalStruct);
BIV_DECL_R (BIV_ScriptStruct);
BIV_DECL_R (BIV_ModuleHandle);
BIV_DECL_R (BIV_MemoryModule);
BIV_DECL_R (BIV_IsDll);
BIV_DECL_R (BIV_IsMini);
BIV_DECL_RW(BIV_CoordMode);
#ifndef MINIDLL
BIV_DECL_R (BIV_IconHidden);
BIV_DECL_R (BIV_IconTip);
BIV_DECL_R (BIV_IconFile);
BIV_DECL_R (BIV_IconNumber);
#endif
BIV_DECL_R (BIV_Space_Tab);
BIV_DECL_R (BIV_AhkVersion);
BIV_DECL_R (BIV_AhkPath);
BIV_DECL_R (BIV_AhkDir);
BIV_DECL_R (BIV_DllPath);
BIV_DECL_R (BIV_DllDir);
BIV_DECL_R (BIV_TickCount);
BIV_DECL_R (BIV_Now);
#ifdef CONFIG_WIN9X
BIV_DECL_R (BIV_OSType);
#endif
BIV_DECL_R (BIV_OSVersion);
BIV_DECL_R (BIV_Is64bitOS);
BIV_DECL_R (BIV_Language);
BIV_DECL_R (BIV_UserName_ComputerName);
BIV_DECL_RW(BIV_WorkingDir);
BIV_DECL_R (BIV_InitialWorkingDir);
BIV_DECL_R (BIV_WinDir);
BIV_DECL_R (BIV_Temp);
BIV_DECL_R (BIV_ComSpec);
BIV_DECL_R (BIV_SpecialFolderPath); // Handles various variables.
BIV_DECL_R (BIV_MyDocuments);
BIV_DECL_R (BIV_Caret);
BIV_DECL_R (BIV_Cursor);
BIV_DECL_R (BIV_ScreenWidth_Height);
BIV_DECL_R (BIV_ScriptName);
BIV_DECL_R (BIV_ScriptDir);
BIV_DECL_R (BIV_ScriptFullPath);
BIV_DECL_R (BIV_ScriptHwnd);
BIV_DECL_R (BIV_LineNumber);
BIV_DECL_R (BIV_LineFile);
BIV_DECL_R (BIV_LoopFileName);
BIV_DECL_R (BIV_LoopFileShortName);
BIV_DECL_R (BIV_LoopFileExt);
BIV_DECL_R (BIV_LoopFileDir);
BIV_DECL_R (BIV_LoopFilePath);
BIV_DECL_R (BIV_LoopFileFullPath);
BIV_DECL_R (BIV_LoopFileShortPath);
BIV_DECL_R (BIV_LoopFileTime);
BIV_DECL_R (BIV_LoopFileAttrib);
BIV_DECL_R (BIV_LoopFileSize);
BIV_DECL_R (BIV_LoopRegType);
BIV_DECL_R (BIV_LoopRegKey);
BIV_DECL_R (BIV_LoopRegSubKey);
BIV_DECL_R (BIV_LoopRegName);
BIV_DECL_R (BIV_LoopRegTimeModified);
BIV_DECL_R (BIV_LoopReadLine);
BIV_DECL_R (BIV_LoopField);
BIV_DECL_RW(BIV_LoopIndex);
BIV_DECL_R (BIV_ThisFunc);
BIV_DECL_R (BIV_ThisLabel);
#ifndef MINIDLL
BIV_DECL_R (BIV_ThisMenuItem);
BIV_DECL_R (BIV_ThisMenuItemPos);
BIV_DECL_R (BIV_ThisMenu);
BIV_DECL_R (BIV_ThisHotkey);
BIV_DECL_R (BIV_PriorHotkey);
BIV_DECL_R (BIV_TimeSinceThisHotkey);
BIV_DECL_R (BIV_TimeSincePriorHotkey);
BIV_DECL_R (BIV_EndChar);
BIV_DECL_R (BIV_Gui);
BIV_DECL_R (BIV_GuiControl);
BIV_DECL_R (BIV_GuiEvent);
BIV_DECL_R (BIV_ScreenDPI);
#endif
BIV_DECL_RW(BIV_EventInfo);
BIV_DECL_R (BIV_TimeIdle);
BIV_DECL_R (BIV_TimeIdlePhysical);
BIV_DECL_R (BIV_IPAddress);
BIV_DECL_R (BIV_IsAdmin);
BIV_DECL_R (BIV_PtrSize);
#ifndef MINIDLL
BIV_DECL_R (BIV_PriorKey);
#endif

////////////////////////
// BUILT-IN FUNCTIONS //
////////////////////////

#ifdef ENABLE_DLLCALL
bool IsDllArgTypeName(LPTSTR name);
void *GetDllProcAddress(LPCTSTR aDllFileFunc, HMODULE *hmodule_to_free = NULL);
BIF_DECL(BIF_DllCall);
BIF_DECL(BIF_DllImport);
BIF_DECL(BIF_DynaCall);
#endif

BIF_DECL(BIF_Struct);
BIF_DECL(BIF_sizeof);

BIF_DECL(BIF_Getvar);
BIF_DECL(BIF_Alias);
BIF_DECL(BIF_CacheEnable);
BIF_DECL(BIF_getTokenValue);
BIF_DECL(BIF_UnZipRawMemory);
BIF_DECL(BIF_ResourceLoadLibrary);
BIF_DECL(BIF_MemoryLoadLibrary);
BIF_DECL(BIF_MemoryCallEntryPoint);
BIF_DECL(BIF_MemoryGetProcAddress);
BIF_DECL(BIF_MemoryFreeLibrary);
BIF_DECL(BIF_MemoryFindResource);
BIF_DECL(BIF_MemorySizeOfResource);
BIF_DECL(BIF_MemoryLoadResource);
BIF_DECL(BIF_MemoryLoadString);

BIF_DECL(BIF_StrLen);
BIF_DECL(BIF_SubStr);
BIF_DECL(BIF_InStr);
BIF_DECL(BIF_StrSplit);
BIF_DECL(BIF_RegEx);
BIF_DECL(BIF_Ord);
BIF_DECL(BIF_Chr);
BIF_DECL(BIF_Format);
BIF_DECL(BIF_NumGet);
BIF_DECL(BIF_NumPut);
BIF_DECL(BIF_StrGetPut);
BIF_DECL(BIF_IsLabel);
BIF_DECL(BIF_IsFunc);
BIF_DECL(BIF_Func);
BIF_DECL(BIF_IsByRef);
BIF_DECL(BIF_GetKeyState);
BIF_DECL(BIF_GetKeyName);
BIF_DECL(BIF_VarSetCapacity);
BIF_DECL(BIF_FileExist);
BIF_DECL(BIF_WinExistActive);
BIF_DECL(BIF_Round);
BIF_DECL(BIF_FloorCeil);
BIF_DECL(BIF_Mod);
BIF_DECL(BIF_Abs);
BIF_DECL(BIF_Sin);
BIF_DECL(BIF_Cos);
BIF_DECL(BIF_Tan);
BIF_DECL(BIF_ASinACos);
BIF_DECL(BIF_ATan);
BIF_DECL(BIF_Exp);
BIF_DECL(BIF_SqrtLogLn);
BIF_DECL(BIF_DateAdd);
BIF_DECL(BIF_DateDiff);

BIF_DECL(BIF_OnMessage);
BIF_DECL(BIF_OnExitOrClipboard);

#ifdef ENABLE_REGISTERCALLBACK
BIF_DECL(BIF_RegisterCallback);
#endif

#ifndef MINIDLL
BIF_DECL(BIF_StatusBar);

BIF_DECL(BIF_LV_GetNextOrCount);
BIF_DECL(BIF_LV_GetText);
BIF_DECL(BIF_LV_AddInsertModify);
BIF_DECL(BIF_LV_Delete);
BIF_DECL(BIF_LV_InsertModifyDeleteCol);
BIF_DECL(BIF_LV_SetImageList);

BIF_DECL(BIF_TV_AddModifyDelete);
BIF_DECL(BIF_TV_GetRelatedItem);
BIF_DECL(BIF_TV_Get);
BIF_DECL(BIF_TV_SetImageList);

BIF_DECL(BIF_IL_Create);
BIF_DECL(BIF_IL_Destroy);
BIF_DECL(BIF_IL_Add);
#endif
BIF_DECL(BIF_Trim); // L31: Also handles LTrim and RTrim.

BIF_DECL(BIF_Type);


BIF_DECL(BIF_IsObject);
BIF_DECL(BIF_Object);
BIF_DECL(BIF_Array);
BIF_DECL(BIF_CriticalObject);
BIF_DECL(BIF_ObjAddRefRelease);
BIF_DECL(BIF_ObjBindMethod);
BIF_DECL(BIF_ObjRawSet);
// Built-ins also available as methods -- these are available as functions for use primarily by overridden methods (i.e. where using the built-in methods isn't possible as they're no longer accessible).
BIF_DECL(BIF_ObjXXX);

// Expression operators implemented via SYM_FUNC:
BIF_DECL(Op_ObjInvoke);
BIF_DECL(Op_ObjGetInPlace);
BIF_DECL(Op_ObjIncDec);
BIF_DECL(Op_ObjNew);


// Advanced file IO interfaces
BIF_DECL(BIF_FileOpen);


// COM interop
BIF_DECL(BIF_ComObject);
BIF_DECL(BIF_ComObjActive);
BIF_DECL(BIF_ComObjCreate);
BIF_DECL(BIF_ComObjGet);
BIF_DECL(BIF_ComObjDll);
BIF_DECL(BIF_ComObjConnect);
BIF_DECL(BIF_ComObjError);
BIF_DECL(BIF_ComObjTypeOrValue);
BIF_DECL(BIF_ComObjFlags);
BIF_DECL(BIF_ComObjArray);
BIF_DECL(BIF_ComObjQuery);


BIF_DECL(BIF_Exception);


BIF_DECL(BIF_WinGet);
BIF_DECL(BIF_WinSet);
BIF_DECL(BIF_WinRedraw);
BIF_DECL(BIF_WinMoveTopBottom);
BIF_DECL(BIF_Process);
BIF_DECL(BIF_ProcessSetPriority);
BIF_DECL(BIF_MonitorGet);

BIF_DECL(BIF_PerformAction);


#define BIF_DECL_STRING_PARAM(n, name) \
	TCHAR name##_buf[MAX_NUMBER_SIZE], \
	*name = (aParamCount >= n ? TokenToString(*aParam[n-1], name##_buf) : _T(""))


BOOL ResultToBOOL(LPTSTR aResult);
BOOL VarToBOOL(Var &aVar);
BOOL TokenToBOOL(ExprTokenType &aToken);
SymbolType TokenIsNumeric(ExprTokenType &aToken);
SymbolType TokenIsPureNumeric(ExprTokenType &aToken, SymbolType &aNumType);
BOOL TokenIsEmptyString(ExprTokenType &aToken);
BOOL TokenIsEmptyString(ExprTokenType &aToken, BOOL aWarnUninitializedVar); // Same as TokenIsEmptyString but optionally warns if the token is an uninitialized var.
__int64 TokenToInt64(ExprTokenType &aToken);
double TokenToDouble(ExprTokenType &aToken, BOOL aCheckForHex = TRUE);
LPTSTR TokenToString(ExprTokenType &aToken, LPTSTR aBuf = NULL, size_t *aLength = NULL);
ResultType TokenToDoubleOrInt64(const ExprTokenType &aInput, ExprTokenType &aOutput);
IObject *TokenToObject(ExprTokenType &aToken); // L31
Func *TokenToFunc(ExprTokenType &aToken);
ResultType TokenSetResult(ResultToken &aResultToken, LPCTSTR aValue, size_t aLength = -1);

LPTSTR RegExMatch(LPTSTR aHaystack, LPTSTR aNeedleRegEx);
void SetWorkingDir(LPTSTR aNewDir, bool aSetErrorLevel = true);
int ConvertJoy(LPTSTR aBuf, int *aJoystickID = NULL, bool aAllowOnlyButtons = false);
bool ScriptGetKeyState(vk_type aVK, KeyStateTypes aKeyStateType);
bool ScriptGetJoyState(JoyControls aJoy, int aJoystickID, ExprTokenType &aToken, LPTSTR aBuf);

HWND DetermineTargetWindow(ExprTokenType *aParam[], int aParamCount);

LPTSTR GetExitReasonString(ExitReasons aExitReason);

#endif
