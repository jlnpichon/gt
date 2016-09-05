gt
====

gt is a minimal toy implementation of the distributed SCM git.
It takes the basics [design and concepts][1] of git, that is:
* blob
* commit
* tree

build
=====
``` sh
$ make
```

usage
=====
Set up the directory where gt is going to store his objects
``` sh
$ mkdir -p ./.gt/objects
```

Compute object ID and store the blob file in the "database"
``` sh
$ echo "file content" | ./hash-blob --write
d1ccbc3bf5c306c3334abcc5465ef32ee7914c77

$ tree .gt
.gt
└── objects
    └── d1
        └── ccbc3bf5c306c3334abcc5465ef32ee7914c77

2 directories, 1 file
```

Add files to the staging area (a.k.a the index). It also writes files as blob
objects into the database
``` sh
$ echo "my file to add in the index" > file.txt
$ ./update-index --add --verbose -- file.txt
78abe3733f50fb48969f05410823fd83669de63e file.txt

$ tree .gt
.gt
├── index
└── objects
    └── 78
        └── abe3733f50fb48969f05410823fd83669de63e

2 directories, 2 files
```
Note the creation of the '.gt/index' file

[1]: https://git-scm.com/book/en/v2/Git-Internals-Git-Objects
