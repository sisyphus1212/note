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
digraph left {
    graph [rankdir="LR", splines=ortho];
    node [shape=record];

    l1 [label="A\l|B\l"];
    l2 [label="C\l|short\l"];
    l3 [label="E\l|long long text\l"];

    l1 -> l2;
    l1 -> l3;
}
```
```graphviz
   digraph h {
     rankdir=LR;

     node [nojustify=false,shape=record,height=.08,fontsize=11];
     elk[label="elk|I am an American Elk"];

     buffalo[label="buffalo|Just a buffalo|everywhere I go|people know the part I'm playing"];

     cow[label="cow pp"];
     k[label="5.migration_iteration_run"]
    migration_thread ->  "4.migrate_set_state";
    migration_thread ->  k;
     moose[label="Bullwinkle J. Moose|Hey Rocky, watch me pull a rabbit out of my hat!"];

     zoo [label="zoo|<p0>|<p1>|<p2>|<p3>"];

     zoo:p0 -> elk;
     zoo:p1 -> cow;
     zoo:p2 -> moose;
     zoo:p3 -> buffalo;
   }
```
```graphviz
digraph G {
  node [width=3 shape=box]
  a [nojustify=false label="The first line is longer\nnojustify=false\l"]
  b [nojustify=true label="The first line is longer\nnojustify=true\l"]
  a -> b
  c [nojustify=false shape=record label="{Records Example - Long Line\n | Title - Shorter Line\nnojustify=false\l}"]
  d [nojustify=true shape=record label="{Records Example - Long Line\n | Title - Shorter Line\nnojustify=true\l}"]
  c -> d
}
```
```graphviz
digraph {
  subgraph cluster_l {
    label="l"
    labeljust=l
    a
  }
  subgraph cluster_c {
    label="c"
    labeljust=c
    labelloc=b
    b
  }
  subgraph cluster_r {
    label="r"
    labeljust=r
    c
  }
    migration_thread ->  "4.migrate_set_state";
    migration_thread ->  s[label="5.migration_iteration_run"];
}
```

```graphviz
digraph structs2
{
    rankdir = LR;
    node [shape=record];
    splines=false;       // optional; gives straight lines

    hashTable [label="<f0>0|<f1>1|<f2>2|<f3>3|<f4>4|<f5>5|<f6>6|<f7>7|<f8>8"];
    node_1_0 [label="<f0> one|<f1> two |<f2> three" ];
    node_1_1 [label="<f0> un |<f1> deux|<f2> trois"];
    struct3 [label="<f0> einz|<f1> swei|<f2> drei"];

    //
    //node_1_0 -> node_1_1[ style = invis, weight= 10 ];
    //                    ^^^^^^^^^^^^^^^^^^^^^^^^^

    hashTable:f1 -> node_1_0:f0;
    node_1_0:f2  -> node_1_1:f0;
    hashTable:f4 -> struct3:f0;
}
```
```graphviz
digraph structs2
{
    rankdir = LR;
    node [shape=record];
    splines=false;       // optional; gives straight lines

    hashTable [label="<f0>0|<f1>1|<f2>2|<f3>3|<f4>4|<f5>5|<f6>6|<f7>7|<f8>8"];
    node_1_0 [label="<f0> one|<f1> two |<f2> three" ];
    node_1_1 [label="<f0> un |<f1> deux|<f2> trois"];
    struct3 [label="<f0> einz|<f1> swei|<f2> drei"];

    //
    node_1_0 -> node_1_1[ style = invis, weight= 10 ];
    //                    ^^^^^^^^^^^^^^^^^^^^^^^^^

    hashTable:f1 -> node_1_0:f0;
    node_1_0:f2  -> node_1_1:f0;
    hashTable:f4 -> struct3:f0;
}
```

