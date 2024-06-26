---
title: graphviz
date: 2023-12-21 14:53:25
categories:
- [other]
tags:
- graphviz
---

# 注意1,2,3,4 箭头的起始变化
```sh
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
```graphviz
digraph {
  compound=true;

  subgraph cluster_a {
    label="Cluster A";
    node1; node3; node5; node7;
  }
  subgraph b {
    cluster=true;
    label="Cluster B";
    node2; node4; node6; node8;
  }

  node1 -> node2 [label="1"];
  node3 -> node4 [label="2" ltail="cluster_a"];

  node5 -> node6 [label="3" lhead="b"];
  node7 -> node8 [label="4" ltail="cluster_a" lhead="b"];
}
```

# 合并连线
```sh
digraph {
    concentrate=true
    a -> b [label="1"]
    c -> b
    d -> b
}
```
```graphviz
digraph {
    concentrate=true
    a -> b [label="1"]
    c -> b
    d -> b
}
```

# 连线方向约束
```sh
digraph G {
  a -> c;
  a -> b;
  b -> c [constraint=false];
}
```
```graphviz
digraph G {
  a -> c;
  a -> b;
  b -> c [constraint=false];
}
```

# 连线上标签
```sh
digraph {
  a -> a [label="AA" decorate=true]
  a -> b [label="AB" decorate=true]
  b -> b [label="BB" decorate=false]
}
```
```graphviz
digraph {
  a -> a [label="AA" decorate=true]
  a -> b [label="AB" decorate=true]
  b -> d [label="BB" decorate=false]
  d -> g
    -> c
    -> f

}
```

# 方位控制
```sh
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

# 结构体
```sh
digraph structs {
    node[shape=record];
    graph[rankdir=TB];
        struct1[label="<f0> left|<f1> mid\ dle|<f2> right"];
    struct2[label="<f0> one|<f1> two"];
    struct3[label="hello\nworld |{ b |{c|<here> d|e}| f}| g | h"];
    struct1:f1 -> struct2:f0;
    struct1:f2 -> struct3:here;
    subgraph clusterAnimalImpl{
		bgcolor = "yellow";
		Dog[label = "{Dog| |+ bark() : void\l + main() :void}" , shape = "record"];
		Cat[label = "{Cat| |+ meow() : void\l}" , shape = "record"];
	};
}
```
```graphviz
digraph structs {
    node[shape=record];
    graph[rankdir=TB];
        struct1[label="<f0> left|<f1> mid\ dle|<f2> right"];
    struct2[label="<f0> one|<f1> two"];
    struct3[label="hello\nworld |{ b |{c|<here> d|e}| f}| g | h"];
    struct1:f1 -> struct2:f0;
    struct1:f2 -> struct3:here;
    subgraph clusterAnimalImpl{
		bgcolor = "yellow";
		Dog[label = "{Dog| |+ bark() : void\l + main() :void}" , shape = "record"];
		Cat[label = "{Cat| |+ meow() : void\l}" , shape = "record"];
	};
}
```

# 方向，尺寸，间距
```
digraph G {
    nodesep = 2;
    ranksep = 1;
    rankdir = LR;
    a -> b; c; b -> d;
}
```
```graphviz
digraph G {
    nodesep = 2;
    ranksep = 1;
    rankdir = LR;
    a -> b; c; b -> d;
}
```

# port 属性
```
digraph G {
    a0 -> b0:w;
    a1 -> b1:e;
    a2 -> b2:n;
    a3 -> b3:s;
    a3_1 -> b3_1:nw;
    a3_2 -> b3_2:se;
}
```
```graphviz
digraph G {
    a0 -> b0:w;
    a1 -> b1:e;
    a2 -> b2:n;
    a3 -> b3:s;
    a3_1 -> b3_1:nw;
    a3_2 -> b3_2:se;
}
```

# 颜色控制
style：filled, invisible, diagonals, rounded. dashed, dotted, solid, bold
color:green,blue,lightblue,yellow,grey
```graphviz
digraph G {
  rankdir=LR  node [shape=box, color=blue]
  node1 [style=dotted]
  node2 [style=filled, fillcolor=red]
  node0 -> node1 -> node2
}
```

# 脑图
```graphviz
graph graph_name{
    layout=dot; graph [ranksep=1.5];
	Happiness -- {
		Peace
		Love
		Soul
		Mind
		Life
		Health
	}
    Love -- {
		Giving
		People
		Beauty
	}
}
```

# 例子
```
digraph {
  rankdir=TB

  subgraph cluster_b {
    label="Cluster B";
    {rank=same; node2; node4; node6; node8;}
  }

  subgraph cluster_a {
    label="Cluster A";
    {rank=same; node1; node3; node5; node7;}
  }

  subgraph cluster_c {
    label="Cluster c";
    {rank=same; node11; node12; node15; node17;}
  }
  node1 -> node11 []
  node11 -> node2
}
```
```graphviz
digraph {
  rankdir=TB

  subgraph cluster_b {
    label="Cluster B";
    {rank=same; node2; node4; node6; node8;}
  }

  subgraph cluster_a {
    label="Cluster A";
    {rank=same; node1; node3; node5; node7;}
  }

  subgraph cluster_c {
    label="Cluster c";
    {rank=same; node11; node12; node15; node17;}
  }
  node1 -> node11 []
  node11 -> node2
}
```
```
digraph {
    newrank=true;
    rankdir="LR";
    subgraph clusterA {
        a0 -> a1 -> a2 [style = invis] // set node order in cluster
        a2 -> a0 [constraint=false] //don't use this edge for ranking
    }

    subgraph clusterB {
        b0 -> b1 -> b2 [style = invis]
        b2 -> b0  [constraint=false]
    }

    subgraph clusterc {
        c0 -> c1 -> c2 [style = invis]
        c2 -> c0  [constraint=false]
    }
}
```
```graphviz
digraph {
    newrank=true;
    rankdir="LR";
    subgraph clusterA {
        a0 -> a1 -> a2 [style = invis] // set node order in cluster
        a2 -> a0 [constraint=false] //don't use this edge for ranking
    }

    subgraph clusterB {
        b0 -> b1 -> b2 [style = invis]
        b2 -> b0  [constraint=false]
    }

    subgraph clusterc {
        c0 -> c1 -> c2 [style = invis]
        c2 -> c0  [constraint=false]
    }
}
```
```
digraph graph_name{
    bgcolor="transparent";//背景透明
        subgraph cluster_subgraph_name{//聚集子图
            node[shape=box];
            cluster_A -> cluster_B;
        }
        subgraph subgraph_name{//子图
            node[shape=none];
            sub_A -> sub_B;
        }
        {//匿名子图
            node[shape=octagon];
            nest_A -> nest_B;
        }
        global_A -> global_B;
        cluster_B -> global_B;
        sub_B -> global_B;
        nest_B -> global_B;
}
```
```graphviz
digraph graph_name{
    bgcolor="transparent";//背景透明
        subgraph cluster_subgraph_name{//聚集子图
            node[shape=box];
            cluster_A -> cluster_B;
        }
        subgraph subgraph_name{//子图
            node[shape=none];
            sub_A -> sub_B;
        }
        {//匿名子图
            node[shape=octagon];
            nest_A -> nest_B;
        }
        global_A -> global_B;
        cluster_B -> global_B;
        sub_B -> global_B;
        nest_B -> global_B;
}
```
```graphviz
graph {
  label="Vincent van Gogh Paintings"
  URL="https://en.wikipedia.org/wiki/Vincent_van_Gogh"

  subgraph cluster_self_portraits {
    URL="https://en.wikipedia.org/wiki/Portraits_of_Vincent_van_Gogh"
    label="Self-portraits"

    "Self-Portrait with Grey Felt Hat" [URL="https://www.vangoghmuseum.nl/en/collection/s0016V1962"]
    "Self-Portrait as a Painter" [URL="https://www.vangoghmuseum.nl/en/collection/s0022V1962"]
  }

  subgraph cluster_flowers {
    URL="https://en.wikipedia.org/wiki/Sunflowers_(Van_Gogh_series)"
    label="Flowers"

    "Sunflowers" [URL="https://www.nationalgallery.org.uk/paintings/vincent-van-gogh-sunflowers"]
    "Almond Blossom" [URL="https://www.vangoghmuseum.nl/en/collection/s0176V1962"]
  }
}
```