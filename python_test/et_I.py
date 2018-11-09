from netgen.geom2d import unit_square
from netgen.csg import unit_cube
from trefftzngs import *
import netgen.gui
from ngsolve import *
from prodmesh import *
from ngsolve.solve import Tcl_Eval # for snapshots

order = 3
c = 1
t_start = 0
t_step = 0.02

initmesh = Mesh(TunnelMesh(maxh = 0.06))
RefineAround([0.5,0,0],0.2,initmesh)
RefineAround([0.5,0,0],0.1,initmesh)
for i in range(0,len(initmesh.GetBoundaries())):
    initmesh.ngmesh.SetBCName(i,"neumann")

D = initmesh.dim
if D==3: eltyp = ET.TET
elif D==2: eltyp = ET.TRIG
elif D==1: eltyp = ET.SEGM
intrule = IntegrationRule(eltyp,2*order)
irsize = len(intrule.points)

fes = H1(initmesh, order=order)
u,v = fes.TnT()
gfu = GridFunction(fes)
a = BilinearForm(fes)
a += SymbolicBFI(u*v)
a.Assemble()
Draw(gfu,initmesh,'sol',autoscale=False,min=-0.002,max=0.002)
wavefront = EvolveTentsMakeWavefront(order,initmesh,c,t_start,"gausspw")

input()
for t in range(0,200):
    wavefront = EvolveTents(order,initmesh,c,t_step,wavefront,t_start,"gausspw")

    ipfct=IntegrationPointFunction(initmesh,intrule,wavefront)
    f = LinearForm(fes)
    f += SymbolicLFI(ipfct*v, intrule=intrule)
    f.Assemble()
    gfu.vec.data = a.mat.Inverse(freedofs=fes.FreeDofs()) * f.vec
    Redraw(blocking = True)

    t_start += t_step
    print("time: " + str(t_start))
    # filename = "results/mov/sol"+str(t).zfill(3) +".jpg"
    # Tcl_Eval("Ng_SnapShot .ndraw {};\n".format(filename))
