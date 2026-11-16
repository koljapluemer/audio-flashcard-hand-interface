I want to 3D print a case for a small universal PCB component I soldered. Let's model a rough version of the component first, for reference.

The component is a cut-out from an universal PCB, 11.5mm wide and 25.9mm tall. It's seems about 1.64mm thick, which I guess is standard?
It has a grid of circular holes for through-hole soldering. 
Each hole has a diameter of 1mm.
The holes are around 1.6-1.7mm apart (is this a standard? Can you web search? I can't measure exactly).

The grid 'starts' 0.32mm from the bottom edge and 1.3mm from the left edge. (distance to edge of hole, not center)

On the PCB are soldered 3 simple push buttons, 4 legged.
They have a 6.3mm square base which is 1.82mm tall.
On top of each button base is the actual push button, centered, 3mm diameter, 1mm tall.
The legs are on the outside, such that each button occupies 3 holes of height and 4 holes of width, with the four corner holes in that setup holding the legs.
The lowermost row of the PCB is unoccupied.

I 'threaded' the actual wiring of the buttons through the two unoccupied solder holes at the side of the buttons.
On the left is always connected to ground, on the right is always connected to GPIO. Could you mark these somehow, so we can later reference that?

---

Let's then design the case; which should be made from two snap-fit parts (PLA print probably):

The bottom part should allow for two screw holes (centered as a pair, with a little gap) to attach this case to another component.
I want to put M 1.4 screws here, which have a head of 2.5mm diameter which is 0.75mm tall.

The 6 cables should have groves in the bottom (it's tiny cables, around .8mm thick) and come out straight to the side. 
Since the cables are the only part of the bottom that is disrupting the flat surface the groves should fix that.
Perhaps we can design the two parts of the snap fit so that these holes are split across the two parts, so we don't have to print holes.

Let's also have some strategically placed pins that wedge into unoccupied solder holes of the PCB, so that we can fix it in place.

The top should follow a standard pattern to allow the buttons to be pressed individually, using thinner parts that bend.
The button (covers) should have some tactile design:
The first should be a triangle pointing up, the mid one a circle, the lowest one a triangle pointing down.