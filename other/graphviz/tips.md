---
title: graphviz—tips
date: 2024-05-24 13:00:59
categories:
- [other]
tags:
- graphviz
---
```graphviz
digraph {
  node[shape=rect]
  gateway -> users
  gateway -> companies
  gateway -> groups
}
```
```graphviz
digraph Graph {
  node[shape=rect]
  gateway[width=3 height=0.3]
  gateway -> users
  gateway -> companies
  gateway -> groups
}
```
