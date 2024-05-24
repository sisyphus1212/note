---
title: graphviz—tips
date: 2024-05-24 13:00:59
categories:
- [other]
tags:
- graphviz
---
```sh
digraph {
  node[shape=rect]
  gateway -> users
  gateway -> companies
  gateway -> groups
}
```
```graphviz
digraph {
  node[shape=rect]
  gateway -> users
  gateway -> companies
  gateway -> groups
}
```
```sh
digraph {
  node[shape=rect]
  gateway[width=3 height=0.3]
  gateway -> users
  gateway -> companies
  gateway -> groups
}
```
```graphviz
digraph {
  node[shape=rect]
  gateway[width=3 height=0.3]
  gateway -> users
  gateway -> companies
  gateway -> groups
}
```

```sh
digraph {
  node[shape=rect]
  splines=ortho
  gateway[width=3 height=0.3]
  gateway -> users
  gateway -> companies
  gateway -> groups
}
```
```graphviz
digraph {
  node[shape=rect]
  splines=ortho
  gateway[width=3 height=0.3]
  gateway -> users
  gateway -> companies
  gateway -> groups
}
```

```sh
digraph {
  node[shape=rect]
  splines=ortho
  gateway[width=3 height=0.3]
  gateway -> users
  gateway -> companies
  gateway -> groups
  ranksep=1.5
}
```
```graphviz
digraph {
  node[shape=rect]
  splines=ortho
  gateway[width=3 height=0.3]
  gateway -> users
  gateway -> companies
  gateway -> groups
  ranksep=1.5
}
```
```sh
digraph  {
  node[shape=rect]
  splines=ortho
  gateway[width=3 height=0.3]
  gateway -> users
  gateway -> companies
  gateway -> groups
  nodesep=1
}
```
```graphviz
digraph  {
  node[shape=rect]
  splines=ortho
  gateway[width=3 height=0.3]
  gateway -> users
  gateway -> companies
  gateway -> groups
  nodesep=1
}
```
```sh
digraph  {
  node[shape=rect]
  splines=ortho
  gateway[width=3 height=0.3]
  gateway -> users
  users -> companies
  gateway -> companies
  gateway -> groups
  nodesep=1
}
```
```graphviz
digraph  {
  node[shape=rect]
  splines=ortho
  gateway[width=3 height=0.3]
  gateway -> users
  users -> companies
  gateway -> companies
  gateway -> groups
  nodesep=1
}
```
```sh
digraph  {
  node[shape=rect]
  splines=ortho
  gateway[width=3 height=0.3]
  gateway -> users
  users -> companies
  gateway -> companies
  gateway -> groups
  nodesep=1
  {
    rank=same
    users
    companies
  }
}
```
```graphviz
digraph  {
  node[shape=rect]
  splines=ortho
  gateway[width=3 height=0.3]
  gateway -> users
  users -> companies
  gateway -> companies
  gateway -> groups
  nodesep=1
  {
    rank=same
    users
    companies
  }
}
```

```sh
digraph  {
  node[shape=rect]
  splines=ortho
  gateway[width=3 height=0.3]
  gateway -> users
  users -> companies[constraint=false]
  gateway -> companies
  gateway -> groups
  nodesep=1
}
```
```graphviz
digraph  {
  node[shape=rect]
  splines=ortho
  gateway[width=3 height=0.3]
  gateway -> users
  users -> companies[constraint=false]
  gateway -> companies
  gateway -> groups
  nodesep=1
}
```
```graphviz
   digraph h {
     rankdir=LR;

     node [shape=record,height=.08,fontsize=11];

     elk[label="elk|I am an American Elk"];

     buffalo[label="buffalo|Just a buffalo|everywhere I go|people know the part I'm playing"];

     cow[label="cow|moo"];

     moose[label="Bullwinkle J. Moose|Hey Rocky, watch me pull a rabbit out of my hat!"];

     zoo [label="zoo|<p0>|<p1>|<p2>|<p3>"];

     zoo:p0 -> elk;
     zoo:p1 -> cow;
     zoo:p2 -> moose;
     zoo:p3 -> buffalo;
   }
```
```graphviz
   digraph h {
      node[rankjustify=min]
     rankdir=LR;

     node [shape=record,height=.08,fontsize=11];

     elk[label="elk|I am an American Elk"];

     buffalo[label="buffalo|Just a buffalo|everywhere I go|people know the part I'm playing"];

     cow[label="cow|moo"];

     moose[label="Bullwinkle J. Moose|Hey Rocky, watch me pull a rabbit out of my hat!"];

     zoo [label="zoo|<p0>|<p1>|<p2>|<p3>"];

     zoo:p0 -> elk;
     zoo:p1 -> cow;
     zoo:p2 -> moose;
     zoo:p3 -> buffalo;
   }
```