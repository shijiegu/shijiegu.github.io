---
layout: single
permalink: /jekyll/update/2025/10/31/candy.html
title:  "Halloween project"
date:   2026-10-31
categories: misc
order: 2
sidebar:
  - title: ""
    image: "/assets/images/gpfa_scatter2.gif"
    image_alt: "image"
    nav: sidebar-sample
---
---
Oct 31, 2026

### Motion triggered dropping of a Chinese takeout box full of candies
My neighborhood does this Halloween block party. I am participating first time this year. Organizers gave me some basic direction: make a candy dispenser. Well, here is my take: a motion triggered dropping of a takeout box full of candies from the sky. It is a proud project and I had so much fun designing it and sharing it with my neighborhood. Here, I detail everything so you can create this and build on top of it.

<video width="300" height="450" controls>
  <source src="/assets/videos/demo.mp4" type="video/mp4">
</video>

### Table of Contents
List of Materials.
3D-printing spools.
Code.

### List of Materials
1. (processor) ESP-32S
2. (sensor) tf-luna lidar
3. (motor) NMEA-17 x 2
4. (motor stepper) TB6600 4A 9-42V
5. (sound player) DFPlayer PRO DFR0768 - MP3 player
6. 3.5mm Female Stereo Audio Jack Breakout Board with Bent Pin
7. Fishing line
(Optionally)
8. Fog lights. I used Nilight 4 Inch LED Light Pods Amber Spot Beam Driving Fog Lights.

### 3D-printing spools
#### Open face pancakes rather than a spool
It was very difficult to buy spools of the exact size and so I 3D-printed it myself. Because of the unique hollow out structure of the spool, reinforcing structures must be added in the groove if the CAD design is one single spool object. You can skip adding reinforcing structures if you create two open face pancakes and screw them together to form a spool. This creates very clean product very quickly. I used Claude to generate the spool design. It programmatically draws the object so you can edit the code directly to change parameters. The picture shows the final product. Here are the Python script and the CAD object.

[CAD file]({{ "/assets/files/small_twopartSpool_small.gcode.3mf"}}){: .btn .btn--primary}



