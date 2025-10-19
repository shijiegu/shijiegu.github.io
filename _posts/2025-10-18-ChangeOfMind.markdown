---
layout: single
permalink: /jekyll/update/2025/10/18/change.html
title:  "This is your brain when you change your mind."
date:   2025-10-18
categories: lab
order: 2

sidebar:
  - title: "This is your brain when you change your mind."
    image: "/assets/images/theta.gif"
    image_alt: "image"
    text: "the “change-of-mind” behavior offers a neat window into the mix of now, past, and the future."
    text: "To appear: The hippocampus encodes counterfactual scenarios during change-of-mind behavior, Society for Neuroscience 2025."
    text: "Authors: Shijie Gu, Anna Gillespie, Chenyan Liu, and Loren Frank."
    nav: sidebar-sample
---
Oct 19, 2025

Have you ever morphed your handwritten choice of A into a B when you were in school doing multiple-choice questions? Have you ever picked up a grocery, then put it back right after? My first grad school paper is on this topic - a change of mind. What happens in your brain when you have such a change of mind? Is switching your choice from A to B actually helpful in your next exam? Let me give you some answers in this blog!

<iframe width="560" height="315" src="https://www.youtube.com/embed/FB9uzgxNyck?si=vOScsTjpSF6BdzWW" title="YouTube video player" frameborder="0" allow="accelerometer; autoplay; clipboard-write; encrypted-media; gyroscope; picture-in-picture; web-share" referrerpolicy="strict-origin-when-cross-origin" allowfullscreen></iframe>

### The dataset
Between 2022 to 2024, I “raised” 5 rats in a maze where milk from various ports is always delivered in a specific order across 5 milk spouts. 4 of these spouts are located at the end of 4 long maze arms (1 meter each), extending from the center platform, leading to the home spout, the 5th milk spout. I wanted to see if they can figure this milk delivery pattern out. If so, what is the neural mechanism behind it? 


<figure style="width: 300px" class="align-left">
  <img src="{{ site.url }}{{ site.baseurl }}/assets/images/change_maze.jpeg" alt="">
  <figcaption> The experimental arena looks like this. There are 4 outer arms and a home arm each leading to a milk port. There is a camera in the room to capture animal location.</figcaption>
</figure> 

The way I “raised” them is such that initially, a rat just needs to alternate between going back to home and outer arm milk locations. Once they developed the idea of home vs outer arms, I upgraded the milk delivery pattern to be: Home - 3 - Home - 4 - Home - 2 - Home - 1 - Home 3 and the loop continues. Of course they don’t know this pattern, after 2 weeks of hanging out in this maze, they all figured it out. Their motivation to find this pattern could be to maximize the milk they get. My initial plan of my PhD study is to use this dataset to study how the brain extracts patterns from memories. See more in [this](https://shijiegu.github.io/jekyll/update/2025/10/25/ripple.html) blog post.


### Rats show “change-of-mind” behavior
Here’s something unexpected: rats often change their mind half way through an arm when they forage for milk. 

<figure style="width: 300px" class="align-middle">
  <img src="{{ site.url }}{{ site.baseurl }}/assets/images/change_path.jpg" alt="">
  <figcaption> Two example paths, one with a change mind at arm 2 before heading to arm 4. </figcaption>
</figure> 

It’s something other projects in the lab also observe but the frequency in this task is way higher -- up to 20% of attempts have change of mind insinuation in some rats. There are multiple reasons why my rats do this more than others: my rats are good students who take things seriously; I use a very large maze scale so where the outer arms are 1m rather than the shorter 50cm ones used in our [W maze](https://rnel.rice.edu/pubs/Jadhav%20et%20al_2012_Awake%20Hippocampal%20Sharp-Wave%20Ripples%20Support%20Spatial%20Memory.pdf). This large maze set up creates a large cost of a bad choice.

### “change-of-mind” behavior is corrective
This project is like an indie/alternative part of my PhD, as it started as a small data exploration during a holiday break with no expectations. One of the first things I was curious about are whether the rats were better off keeping their initial choice or changing their mind. It turns out across all rats and all sessions, the choice after a change of mind has on average better outcome. (each day I let them run in the maze for 4-6 sessions of 30-60 min each, with naps in a separate area in between.)
<figure style="width: 300px" class="align-middle">
  <img src="{{ site.url }}{{ site.baseurl }}/assets/images/change_corrective.jpg" alt="">
  <figcaption> Each dot on the plot represents a session's data. In each rat, a dot in the left column is the average correct rate for initial choice on change-of-mind trials; a dot on the right column is the average correct rate for final choice on change-of-mind trials. The final choice is on average better. </figcaption>
</figure> 

So yes. All these efforts to change their mind are worth it! There is more to the story. We analyzed rats’ long-term behavior changes in this maze and found that this increase of correct rate has something to do with the animal overcoming their foraging bias. Stay tuned for the final paper for this detail.


### “change-of-mind” behavior is not preceded by predictive coding in the hippocampus
The hippocampus is a brain structure that is very important for memory formation and retrieval; it is also a brain structure critical for maintaining a so-called “GPS” signal — where you are physically in a space. As a step beyond recalling memory and encoding GPS signal, recently, there have been a few findings linking the hippocampus to the flip side of the past and the current: the future. 

In neuroscience terms, the hippocampus has been linked to predictive coding and mental simulation. In simple terms, the “predictive coding” part says that before an animal makes a choice, you can read out (to some extent) from the hippocampus neural activities what the animal is about to do next; the “mental simulation” part is a mellower version that says before an animal makes a choice, you can read out the few options the animal considered rather than just the specific choice the animal picked. In other words, the field is grappling with how the hippocampus decides to encode where you are now, what you are about to do next, or your past, and how it serves the animal altogether.

This is the point where I realized this project is not indie neuroscience after all — **the “change-of-mind” behavior offers a neat window into the mix of now, past, and the future**. At the tip of a change of mind, does the hippocampus forgo being a GPS and present another option or other options, prompting the rats to change their mind? What is going on in the hippocampus when the rat is actively leaving the initial choice before committing to another one? In the exam analogy, what is going on in the hippocampus while I morph A to B? Does the hippocampus encode the initial choice once the final choice is made? 

To study this, we assume that the hippocampus encodes the GPS signal when the animal is running and create a map that eats in animal body location and returns the average hippocampus signal. We then apply this map in reverse. This reversed function eats in specific moments of hippocampus signal and returns an estimate of the animal body location. If that the moment the hippocampus signal deviates a lot from the usual GPS signal, the estimated body location will not track the actual animal location. It is a mouthful to explain, and it might make more sense to see the plot directly:

<figure style="width: 300px" class="align-middle">
  <img src="{{ site.url }}{{ site.baseurl }}/assets/images/change_thetadecode_lewis.jpg" alt="">
  <figcaption> In the top panel, magenta denotes animal body location and black ink denotes estimates of the animal location from the hippocampus data. In the bottom panel, we plot animal's speed as a reference. The moment when the speed dips to 0 is defined to be the moment the animal changes its mind. </figcaption>
</figure>

To our surprise, there is little loss of GPS signal before a change of mind, defined by animal abruptly stopping on an arm before backing off or turning around to go to another arm. Instead, the hippocampus actively simulates going towards the end of the arm while actively leaving the arm — a mental simulation of what could have been if the choice is not changed. 

I am still doing more analysis to answer more questions. Stay tuned for the final paper!

