import pybullet as p
import time
import pybullet_data

import math

# p.vhacd("data/57BCG2003201508N00.obj", "data/decomp_57BCG2003201508N00.obj", "log.txt")

# exit()


springCount=20


def LoadObj(model,mass,pos,ori,shift,_scale,_color=[1, 0.7, 0.7, 1]):

  # model="bowl.STL"
  # pos=[i * _scale for i in pos]
  # shift=[i * _scale for i in shift]

  scale = [_scale, _scale, _scale]
  visual_shape_id = p.createVisualShape(
      shapeType=p.GEOM_MESH,
      fileName=model,
      rgbaColor=_color,
      specularColor=[1.1,1.1,1.1],
      visualFramePosition=shift,
      meshScale=scale
  )
  flags=0
  if(mass==0):
    flags=p.GEOM_FORCE_CONCAVE_TRIMESH
  collision_shape_id = p.createCollisionShape(
      shapeType=p.GEOM_MESH,
      flags=flags,
      fileName=model,
      collisionFramePosition=shift,
      meshScale=scale
  )


  return p.createMultiBody(
      baseMass=mass,
      baseCollisionShapeIndex=collision_shape_id,
      baseVisualShapeIndex=visual_shape_id,
      basePosition=pos,
      baseOrientation=ori,
      useMaximalCoordinates=True
  )

def aWall():
  visual_shape_id = p.createVisualShape(
    shapeType=p.GEOM_BOX,
    halfExtents=[60, 5, 5]
  )

  collison_box_id = p.createCollisionShape(
      shapeType=p.GEOM_BOX,
      halfExtents=[60, 5, 5]
  )

  return p.createMultiBody(
      baseMass=10000,
      baseCollisionShapeIndex=collison_box_id,
      baseVisualShapeIndex=visual_shape_id,
      basePosition=[0, 10, 5]
  )



def LoadObj_Ghost(model,mass,pos,ori,_scale,_color=[1, 0.7, 0.7, 1]):

  # model="bowl.STL"
  shift = [0,0,0]
  scale = [_scale, _scale, _scale]
  visual_shape_id = p.createVisualShape(
      shapeType=p.GEOM_MESH,
      fileName=model,
      rgbaColor=_color,
      specularColor=[1.1,1.1,1.1],
      visualFramePosition=shift,
      meshScale=scale
  )


  collision_shape_id = p.createCollisionShape(
      shapeType=p.GEOM_BOX,
      halfExtents=[0,0,0]
  )

  return p.createMultiBody(
      baseMass=mass,
      baseCollisionShapeIndex=collision_shape_id,
      baseVisualShapeIndex=visual_shape_id,
      basePosition=pos,
      baseOrientation=ori,
      useMaximalCoordinates=True
  )




physicsClient = p.connect(p.GUI)# p.DIRECT for non-graphical version

p.configureDebugVisualizer(p.COV_ENABLE_RENDERING, 0)

p.configureDebugVisualizer(p.COV_ENABLE_TINY_RENDERER, 0)
p.setAdditionalSearchPath(pybullet_data.getDataPath()) #optionally
p.setGravity(0,0,-10)
# planeId = p.loadURDF("plane.urdf")
# p.changeDynamics(planeId, -1, lateralFriction=0.01,linearDamping=0.2, spinningFriction=0, rollingFriction=0)
GSCALE=0.01
modelvbowlId=LoadObj("data/23042002.STL",0,[0,0,0],p.getQuaternionFromEuler([math.pi/2,0,math.pi/2]), [-0.2, -0.2, -0],GSCALE,[0.7, 0.7, 0.7, 1])
#modelvbowlId=LoadObj("data/Untitled-62aab7cd.stl",0,[-1-0.5,1-1.1,-0.46],p.getQuaternionFromEuler([math.pi/2,0,math.pi*50/180]),0.01,[0.7, 0.7, 0.7, 1])

# airBlowIndId=LoadObj("data/shape/cone.obj",0,[-1,0,0.3],p.getQuaternionFromEuler([0,0,math.pi*45/180]),0.02,[0.5, 0.2, 0.7, 0.4])
# modelvbowlId=LoadObj("data/vbowl3.obj",0,[-1,1,-0.46],p.getQuaternionFromEuler([math.pi/2,0,0]),0.01,[0.7, 0.7, 0.7, 1])


# modelId = p.loadURDF("obj2.urdf",globalScaling=0.01,useFixedBase=1,baseOrientation=p.getQuaternionFromEuler([math.pi/2,0,0]))
# initPos, oldOrn = p.getBasePositionAndOrientation(modelId)
# print( initPos, oldOrn)


p.changeDynamics(modelvbowlId, -1, lateralFriction=0, spinningFriction=0, rollingFriction=0)
# modelId2=LoadObj("decomp.obj",1,[1,1,2],0.03)
# modelId3=LoadObj("testspring.obj",1,[0,0,2],0.03)
# modelId2=LoadObj("testspring.obj",1,[0.1,0,2],0.03)
# p.vhacd("57BCG2004705710N10.obj", "decomp_spring.obj", "log.txt")

sidArr = []

totalSpawn=0
countPerLayer=100
H=0.3
while True:
  curLayerCount=countPerLayer
  if(springCount-totalSpawn<countPerLayer):
    curLayerCount=springCount-totalSpawn
  
  H+=0.1
  for i in range (curLayerCount):
    
    R=0.13+0.007*i
    t=math.pi*2*(1+3*math.pow(i/10,0.3))
    x=math.sin(t)
    y=math.cos(t)

    sid = LoadObj("data/decomp_58B5S2003003108N00.obj",1,
      [R*x,R*y,H+i*0.001],
      p.getQuaternionFromEuler([math.pi/2,0,0]),
      [-1.5*GSCALE,-1.5*GSCALE,-1.5*GSCALE],
      GSCALE,[0.83, 0.686, 0.22, 1])
    sidArr.append(sid)

    p.changeDynamics(sid, -1,lateralFriction=0.2,linearDamping=0.95)
  
  totalSpawn+=curLayerCount
  if(totalSpawn==springCount):
    break

# p.changeDynamics(modelId2, -1, lateralFriction=0.8, spinningFriction=0, rollingFriction=0)
# modelId2=LoadObj("decomp.obj",1,[0,0.1,1],0.003)
# aWall()
# new_pos, new_orn = p.multiplyTransforms(initPos, [0.7071,0,0,0.7071], initPos,oldOrn)

# p.resetBasePositionAndOrientation(modelId, initPos, p.getQuaternionFromEuler([math.pi/2,0,0]))
# modelId = p.loadURDF("bowl.urdf",cubeStartPos, cubeStartOrientation)

p.setRealTimeSimulation(0)
p.configureDebugVisualizer(p.COV_ENABLE_RENDERING, 1)

subStep=2
timeStep=1./200./subStep
slowDownX=0.5
ttTime=0
ttTimeSkip=0
# p.setTimeStep(timeStep)
p.setPhysicsEngineParameter(fixedTimeStep=timeStep)
totPortalCount=0
portalCD=0



vibVDefault=0.1
vibSlider_id=p.addUserDebugParameter("vibV",0,1,vibVDefault)

while True:
  p.stepSimulation()
  ttTime+=timeStep


  valpha=vibVDefault#p.readUserDebugParameter(vibSlider_id)
  respawnSep=3
  for sid in sidArr:
    sPos, sOrn = p.getBasePositionAndOrientation(sid)
    if(portalCD==0 and sPos[2]<-0.2):
      sPos=[0.1,-0.1+(0.1*(totPortalCount%respawnSep)),0.1]
      p.resetBasePositionAndOrientation(sid,sPos,sOrn)
      p.resetBaseVelocity(sid, [0, 0, 0], [0, 0, 0])
      totPortalCount+=1
      portalCD=3000//respawnSep/subStep

      continue
    if(portalCD>0):
      portalCD-=1
    x=sPos[0]
    y=sPos[1]
    absxy=math.hypot(x,y)
    x/=absxy
    y/=absxy
    nx=y
    ny=-x
    
    RAlpha=1
    CWAlpha=1
    x=(RAlpha*0+CWAlpha*1)*valpha/vibVDefault*subStep
    y=(RAlpha*0+CWAlpha*0)*valpha/vibVDefault*subStep
    p.applyExternalForce(objectUniqueId=sid, linkIndex=-1,
                       forceObj=[x,y,0], posObj=sPos, flags=p.WORLD_FRAME)
  # mPos, mOrn = p.getBasePositionAndOrientation(modelId)
#  print( mPos, mOrn)

  
#  forceV=mPos[0]
#  if(forceV<0 and forceV>-1):forceV=-1
#  if(forceV<0):forceV*=3
#  if(forceV>0 and forceV<1):forceV=1

#  forceV=-forceV*3
#  p.applyExternalForce(objectUniqueId=modelId, linkIndex=-1,
#                        forceObj=[forceV,0,0], posObj=mPos, flags=p.WORLD_FRAME)


#  p.applyExternalTorque(objectUniqueId=modelId, linkIndex=-1,
#                        torqueObj=[5,0,0], flags=p.WORLD_FRAME)

  if(False):
    timealpha=1
    ttTANG=math.pi*2*ttTime*timealpha+ttTimeSkip
    if(math.cos(ttTANG)<0):
      ttTimeSkip+=math.pi
      ttTANG+=math.pi

    motionP=math.sin(ttTANG)
    motionD=math.cos(ttTANG)

    rotZalpha=math.pi/2*0.03
    rotZ=rotZalpha*motionP
    angZ=1.2*rotZalpha*motionD #derivative

    posZalpha=0.01
    posZ=1+posZalpha*motionP
    volZ=  posZalpha*motionD

    p.resetBasePositionAndOrientation(modelId,[ initPos[0],initPos[1],posZ],p.getQuaternionFromEuler([-math.pi/2,0,rotZ]))

    p.resetBaseVelocity(modelId, [0, 0, volZ], [0, 0, angZ])
  else:
    None
    # p.resetBasePositionAndOrientation(modelId,initPos,p.getQuaternionFromEuler([math.pi/2,0,0]))

    # p.resetBaseVelocity(modelId, [0, 0, 0], [0, 0, 0])
  # time.sleep(timeStep*slowDownX)
# cubePos, cubeOrn = p.getBasePositionAndOrientation(boxId)
# print(cubePos,cubeOrn)


p.disconnect()