# swan

The <u>sw</u>iss <u>a</u>rmy k<u>n</u>ife program for Windows. One program to replace many, featuring:

- [ ] Powerful [file explorer](#explorer) (wayyy better than Windows Explorer)
- [ ] Super fast file finder (Windows Explorer? pfff)
- [ ] yt-dlp frontend
- [ ] Fast, useful terminal (unlike Command Prompt)

## Explorer

### Controls

entry = file | directory

- \[Left Click] to set selection to entry
- \[Ctrl] + \[Left Click] to toggle entry selection
- \[Shift] + \[Left Click] to add all entries inclusively between prev-click and current-click to selection
- double \[Left Click] to open entry
- \[Enter] to open selected entry
- \[Ctrl] + \[r] to refresh entries

### What annoys me about Windows Explorer

- Only a single pane
- Bad Shift-click behavior
- Search is wayyy too slow (it's useless)
- No batch renaming
- Lacks ability to see/kill processes which are locking a file
- Can't tell if a folder has stuff inside (files? folders? nested folders?)

### Todo

- [x] Remove "." directory
- [x] Double click directory
- [x] Double click file
- [x] Multiple panes
- [x] Manual refresh
- [x] Regex filtering
- [x] Clickable directories in cwd
- [x] Back/forward history buttons
- [x] Ctrl-r to refresh
- [x] "Contains" filtering
- [x] Handle shortcuts (IShellLink)
- [x] History browser
- [ ] Sort directory entries
- [ ] Pins
- [ ] Move directory entries
- [ ] Copy directory entries
- [ ] Glob filtering
- [ ] Customizable context menu
- [ ] Show directory preview (num child files/dirs)
- [ ] File preview
- [ ] Automatic refresh
