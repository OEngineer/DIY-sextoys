/*description:

This is a 3D OpenSCAD copy of the Dr Johnson Vac-U-Lock system.

There is full details at <http://adultblogz.co.uk/Vac-U-Lock>

Added a 1/4" NPTF socket at the bottom for a quick-release air fitting for fucking machine use.
*/
include <threads.scad>

Shaft = 70;		// Length;  min = 70
RodDia = 25.4 / 2 + 0.3;
RodHt = 40;
$fn = 30;
BevelHt = 2.5;  // 3 is about 45 degrees

module vac_u_lock()
{
    	union()
	{
		// The Top bit
		translate([0,0,Shaft])
		sphere(r = 10.87);

		// First Overhang
		translate([0,0,Shaft-24.5])
		cylinder(h = 24.5, r1 = 13.72, r2 = 10.87);

        // bevel
		translate([0,0,Shaft-24.5-BevelHt])
		cylinder(h = BevelHt, r1 = 10.76, r2 = 13.72);
        
		// Second Overhang
		translate([0,0,Shaft-24.5-20.32])
		cylinder(h = 20.32, r1 = 12.7, r2 = 10.76);		
		
        // bevel
		translate([0,0,Shaft-24.5-20.32-BevelHt])
		cylinder(h = BevelHt, r1 = 10.16, r2 = 12.7);
        
		// Shaft
		cylinder(h = Shaft, r = 10.16);	
	}
}

difference()
{
    union() {
        vac_u_lock(); 
        // bottom plate for adhesion
        // cylinder(h = 5, r = 17);
    }

    // add 0.3mm flat at base for adhesion
    translate([0,0,0.3])
    english_thread (diameter=0.54, threads_per_inch=18, length=5/8, taper=1/16, internal=true);

 }
