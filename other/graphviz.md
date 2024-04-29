---
title: graphviz
date: 2023-12-21 14:53:25
categories:
- [other]
tags:
- graphviz
---

# 注意1,2,3,4 箭头的起始变化
```graphviz
digraph {
  compound=true;

  subgraph cluster_a {
    label="Cluster A";
    node1; node3; node5; node7;
  }
  subgraph cluster_b {
    label="Cluster B";
    node2; node4; node6; node8;
  }

  node1 -> node2 [label="1"];
  node3 -> node4 [label="2" ltail="cluster_a"];

  node5 -> node6 [label="3" lhead="cluster_b"];
  node7 -> node8 [label="4" ltail="cluster_a" lhead="cluster_b"];
}
```

# 合并连线
```graphviz
digraph {
    concentrate=true
    a -> b [label="1"]
    c -> b
    d -> b
}
```

# 连线方向约束
```graphviz
digraph G {
  a -> c;
  a -> b;
  b -> c [constraint=false];
}
```

# 连线上标签
```graphviz
digraph {
  a -> a [label="AA" decorate=true]
  a -> b [label="AB" decorate=true]
  b -> b [label="BB" decorate=false]
}
```

# ---
```graphviz
digraph {
    rankdir=TB
    subgraph cluster_vertical_example2{
        Node1; Node6
    }
    subgraph cluster_vertical_example{
        "Node2"-> "Node3";
        "Node3"-> "Node4";
        "Node4"->"Node5"
        {rank=same; "Node4"; "Node5";}
    }
    Node1 -> Node2 [constraint=false, dir=back]
}
```
```graphviz
digraph {
subgraph cluster_0 { A1 → A2 → A3 }
subgraph cluster_1 { B1 → B2 → B3 }
subgraph cluster_2 { C1 → C2 → C3 }
subgraph cluster_3 { D1 → D2 → D3 }
subgraph cluster_4 { E1 → E2 → E3 }
{ A1 B1 } → C1 → { D1 E1 }
}
```