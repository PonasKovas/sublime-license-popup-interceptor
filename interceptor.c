#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>

// The text by which we identify the license popup among other potential popups.
const char* LICENSE_TEXT = "Hello! Thanks for trying out Sublime Text.\n\nThis is an unregistered evaluation version, and although the trial is untimed, a license must be purchased for continued use.\n\nWould you like to purchase a license now?";

typedef void* (*dlsym_t)(void* restrict, const char* restrict);
typedef void* (*gtk_message_dialog_new_t)(void*, int, int, int, const char*, ...);
typedef void (*gtk_widget_show_t)(void*);

static dlsym_t dlsym_f = NULL;
static gtk_message_dialog_new_t gtk_message_dialog_new_f = NULL;
static gtk_widget_show_t gtk_widget_show_f = NULL;

// The widget id of the last created license popup. This ID will be blocked from being displayed
static void* last_known_license_widget_id = NULL;

// Simple logging function for debugging. Cant just print to stdout because sublime deattaches itself
void log_interceptor(const char* format, ...) {
	FILE *file = fopen("/tmp/sublime-license-interceptor.log", "a");
    if (!file) {
        perror("Failed to open file");
        exit(EXIT_FAILURE);
    }

    va_list args;
    va_start(args, format);
    vfprintf(file, format, args);
    va_end(args);

    fclose(file);  
}

// Initializes the pointer to the real dlsym function
void init_dlsym_f() {
	if (dlsym_f == NULL) {
		void* real_dlsym = dlvsym(RTLD_NEXT, "dlsym", "GLIBC_2.34");
		if (real_dlsym == NULL) {
			log_interceptor("real dlsym not found\n");
			exit(1);
		}
		dlsym_f = (dlsym_t) real_dlsym;
	}
}

// initalizes the pointers to the real gtk functions
void init_gtk_f() {
	if (gtk_message_dialog_new_f == NULL) {
		init_dlsym_f();

		void* gtk_handle = dlopen("libgtk-3.so", RTLD_LAZY);
		if (gtk_handle == NULL) {
			log_interceptor("failed to load gtk\n");
			exit(1);
		}

		void* real = dlsym_f(gtk_handle, "gtk_message_dialog_new");
		if (real == NULL) {
			log_interceptor("real gtk_message_dialog_new not found\n");
			exit(1);
		}
		gtk_message_dialog_new_f = (gtk_message_dialog_new_t) real;

		real = dlsym_f(gtk_handle, "gtk_widget_show");
		if (real == NULL) {
			log_interceptor("real gtk_widget_show not found\n");
			exit(1);
		}
		gtk_widget_show_f = (gtk_widget_show_t) real;
	}
}

// wrapper over the real gtk_message_dialog_new
// all it does it is check if the text of the popup matches what we need, and in that case saves the widget id
// so the widget wouldnt be displayed later.
void* gtk_message_dialog_new(void* parent, int flags, int msg_type, int buttons, const char* fmt, ...) {
	init_gtk_f();

	// i dont know how to handle this if this is called with anything else than "%s"
	// is it even possible to call the real gtk function with those same args?
	if (strcmp("%s", fmt) != 0) {
		log_interceptor("unexpected fmt: '%s'\n", fmt);
		return NULL;
	}

	// get the first and only argument
	va_list args;
    va_start(args, fmt);
    const char *first_arg = va_arg(args, const char *);
    va_end(args);      

    // run the real gtk function
    void* widget_id = gtk_message_dialog_new_f(parent, flags, msg_type, buttons, fmt, first_arg);

   	if (strcmp(LICENSE_TEXT, first_arg) == 0) {
		// found the license popup
		log_interceptor("found the bastard\n");
		last_known_license_widget_id = widget_id;
	}

	return widget_id;
}

// Sublime uses this function to display the license popup
// so whenever it is called, we check if its the same widget ID that we have saved
// as belonging to a license popup, and if it is, we ignore this call
void gtk_widget_show(void* widget) {
	init_gtk_f();
	
	if (last_known_license_widget_id == widget) {
		log_interceptor("intercepting gtk_widget_show bastard\n");
		return;
	}

	return gtk_widget_show_f(widget);
}

// A wrapper around dlsym, which sublime uses to get the function pointers of gtk
void* dlsym(void* restrict handle, const char *restrict symbol) {
	if (strcmp("gtk_message_dialog_new", symbol) == 0) {
		return (void*)gtk_message_dialog_new;
	}
	if (strcmp("gtk_widget_show", symbol) == 0) {
		return (void*)gtk_widget_show;
	}

	init_dlsym_f();
	
	return dlsym_f(handle, symbol);
}

