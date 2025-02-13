# Known Issues

- lack of code separation via separate files
- not enough testing for latency, disconnects, and potential editing conflicts
- - doubtful zeromq fixes all of this out of the box
- usage of czmq library potentially hiding some of the workings of zeromq

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

redis,
why redis,
big string and csv decoding

redis 2d list granular update
zeromq pub sub

why zeromq pub sub over redis pub sub
automatic message queuing among other features

no malloc? lets make it use malloc
first attempt: seg fault!
