# Build and Run Instructions

1. build the shared library for raylib following the instructions for your platform 
    https://github.com/raysan5/raylib/wiki/Working-on-GNU-Linux
2. on debian/ubuntu, make sure `build-essential uuid-dev libczmq-dev` is installed
3. run the build.sh script for the client, and then cd to server directory and run buil.sh there
4. run server, then run several clients to see them sync with each other.

You may control the camera with arrow keys and scroll to zoom.

# Potential/Known Issues

- not enough testing for latency, disconnects, and potential editing conflicts at scale
- - doubtful zeromq fixes all of this out of the box
- could use different build config for debug and release

# Dev Blog

I started this project to learn about zeromq and how it can be used to create a collaborative editing environment,
similar to Google Docs. I have heard zeromq be praised by colleagues in the past, and I wanted to see how it can
help me resolve the inevitable conflicts that arise in an application where many clients are trying to update state
concurrently.

Rather than create something exactly like a collaborative notepad right away, I settled on creating a sort of
"tilemap editor" that has now become more like a pixel art editor, as I didn't feel like using images of tiles
for this project. This could perhaps come later.

I needed to choose a way to draw on the screen.  Ideally something compatible with webassembly, although I am umsure
how emscripten handles zeromq client sockets that use tcp.  While I really want to practice my graphics programming
skills, I figured Raylib would be most sufficient for this project to help me draw things quickly and focus on
the goals of the project.  In the future, I may migrate to a lower level graphics library, but this is low priority.

began zeromq req/resp

big string and csv decoding

zeromq pub sub

why zeromq pub sub is unique over other option?
because automatic message queuing among other features

no malloc? lets make it use malloc
another option would be to use a pre-allocated buffer of maximum possible size (100x100)
first attempt: seg fault!

rewrite the resize realloc more carefully, works like a charm
adding zoom and camera controls

finding memory leaks
