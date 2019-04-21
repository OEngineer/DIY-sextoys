/* Description:

This is a 3D OpenSCAD copy of the Dr Johnson Vac-U-Lock system.

There is full details at <http://adultblogz.co.uk/Vac-U-Lock>
*/

Shaft = 110;		// Length;  min = 70
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
        cylinder(h = 5, r = 17);
    }
    translate([0,0,-1])
        cylinder(r=RodDia/2, h=RodHt);
 }
