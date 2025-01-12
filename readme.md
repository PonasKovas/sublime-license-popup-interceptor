# sublime license popup interceptor

For educational purposes only.

Linux only as of now, but could be expanded to more platforms potentially quite easily.

This simple program demonstrates how the Sublime Text license popup can be disabled by intercepting GTK calls.

## Usage

```sh
LD_PRELOAD=/path/to/interceptor.so /opt/sublime_text/sublime_text
```

You can edit your sublime text .desktop file to add this environment variable and it should work.