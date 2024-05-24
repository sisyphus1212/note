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
  digraph left {
      graph [rankdir="LR", splines=ortho];
      node [shape=record];
      node[rankjustify=min]
      l1 [label="A\l|B\l"];
      l2 [label="C\l|short\l"];
      l3 [label="E\l|long long text\l"];

      l1 -> l2;
      l1 -> l3;
  }
```