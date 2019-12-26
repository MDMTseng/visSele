from random import seed
from random import random
# seed random number generator
seed(1)


name="Cut_"+str(random())

App.ActiveDocument.addObject("Part::Box","Box")
App.ActiveDocument.ActiveObject.Label = "Cube"
App.ActiveDocument.recompute()
Gui.SendMsgToActiveView("ViewFit")
FreeCAD.getDocument("Unnamed").getObject("Box").Placement = App.Placement(App.Vector(0,9,0),App.Rotation(App.Vector(0,0,1),0))
App.ActiveDocument.addObject("Part::Cylinder","Cylinder")
App.ActiveDocument.ActiveObject.Label = "Cylinder"
App.ActiveDocument.recompute()
Gui.SendMsgToActiveView("ViewFit")
FreeCAD.getDocument("Unnamed").getObject("Box").Placement = App.Placement(App.Vector(0,0,0),App.Rotation(App.Vector(0,0,1),0))

App.activeDocument().addObject("Part::Cut",name)
App.activeDocument()[name].Base = App.activeDocument().Box
App.activeDocument()[name].Tool = App.activeDocument().Cylinder
Gui.activeDocument().hide("Box")
Gui.activeDocument().hide("Cylinder")
Gui.ActiveDocument[name].ShapeColor=Gui.ActiveDocument.Box.ShapeColor
Gui.ActiveDocument[name].DisplayMode=Gui.ActiveDocument.Box.DisplayMode
