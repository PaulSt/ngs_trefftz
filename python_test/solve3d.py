#########################################################################################################################################
c = 1
basemeshsize = 1/4
t_stepsize = 1/4
nt_steps =1
order = 5
k = 1
#########################################################################################################################################
from netgen.geom2d import unit_square
from netgen.csg import unit_cube
from prodmesh import *
from ngsolve import *
import netgen.gui
from trefftzngs import *
from DGeq import *
import time
# mesh = Mesh(unit_cube.GenerateMesh(maxh = 0.2,quad_dominated=True))
ngmeshbase = unit_square.GenerateMesh(maxh = basemeshsize)
mesh = ProdMesh(ngmeshbase,t_stepsize)
#########################################################################################################################################
truesol =   sin( k*(c*z+x+y) )#exp(-100*((x-0.5)*(x-0.5)+(y-0.5)*(y-0.5)) )  sin( k*(c*z+x+y) )
# v0 = 0 #c*k*cos(k*(c*z+x+y))#grad(U0)[0]
# sig0 = CoefficientFunction( (200 * (x-0.5) * truesol, 200 * (y-0.5) * truesol)) #CoefficientFunction( (-k*cos(k*(c*z+x+y)),-k*cos(k*(c*z+x+y))) )# #-grad(U0)[1]

# fes = Periodic(FESpace("trefftzfespace", mesh, order = order, wavespeed = c, dgjumps=True, basistype=0))
fes =(FESpace("trefftzfespace", mesh, order = order, wavespeed = c, dgjumps=True, basistype=0))
# fes = L2(mesh, order=order, dgjumps = True )

basemesh = Mesh(ngmeshbase)
gfu = GridFunction(fes)
gfu.Set(truesol)
# Draw(gfu,mesh,'sol',draw_surf=False)
for t in range(nt_steps):
    # truesol = exp(-100*((x-0.5)*(x-0.5)+(y-0.5)*(y-0.5)) ) #sin( k*( c*(z+t*t_stepsize) +x+y) )
    gD = grad(gfu)[0]#c*k* cos( k*( c*(z+t*t_stepsize) +x+y) )
    # v0 = c*k*cos(k*(c*(z+t*t_stepsize)+x+y))
    # sig0 = CoefficientFunction( (-k*cos(k*(c*(z+t*t_stepsize)+x+y)),-k*cos(k*(c*(z+t*t_stepsize)+x+y))) )

    start = time.clock()
    [a,f] = DGeqsysperiodic(fes,gfu,c,gD)
    print("DGsys: ", str(time.clock()-start))

    start = time.clock()
    [gfu, cond] = DGsolve(fes,a,f)
    print("DGsolve: ", str(time.clock()-start))

    # v0 = grad(gfu)[2]
    # sig0 = CoefficientFunction( (-grad(gfu)[0],-grad(gfu)[1]) )

    L2error = sqrt(Integrate((truesol - gfu)*(truesol - gfu), mesh))
    gradtruesol = CoefficientFunction(( k*cos(k*(c*z+x+y)), k*cos(k*(c*z+x+y)), c*k*cos(k*(c*z+x+y)) ))
    sH1error = sqrt(Integrate((gradtruesol - grad(gfu))*(gradtruesol - grad(gfu)), mesh))
    print("L2error=", L2error)
    print("grad-error=", sH1error)

    # Draw(grad(gfu)[2],mesh,'grad')
    # Draw(gfu,mesh,'gfu')
    # Draw(grad(gfu),mesh,'gradgfu')
    # input()
    # Draw(v0,basemesh,'gradtime')
    # gfu = ClipCoefficientFunction(gfu,2,t_stepsize)
    Draw(gfu,mesh,'sol',draw_surf=False)
    Draw(grad(gfu),mesh,'gsol',draw_surf=False)
    input(t)

# Draw(gfu,mesh,'sol')
# Draw(grad(gfu),mesh,'gradsol')

#
#Draw(gfu,basemesh,'sol')

#for time in np.arange(0.0, t_stepsize, 0.1):
#l2fes = L2(basemesh,order=order)
#foo = GridFunction(l2fes)

#for p in ngmeshbase.Points():
#	gfu(p.p)




