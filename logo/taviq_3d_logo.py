# Procedural, non-AI 3D logo generator for Blender 3.6+
# Run: blender -b -P taviq_3d_logo.py
import bpy, math
from mathutils import Vector

bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)

def mat(name, color, metallic=0.0, roughness=0.35):
    m = bpy.data.materials.new(name)
    m.diffuse_color = (*color, 1)
    m.use_nodes = True
    bs = m.node_tree.nodes.get("Principled BSDF")
    bs.inputs["Base Color"].default_value = (*color, 1)
    bs.inputs["Metallic"].default_value = metallic
    bs.inputs["Roughness"].default_value = roughness
    return m

silver = mat("Brushed Titanium", (0.42, 0.62, 0.78), 0.88, 0.22)
dark = mat("Midnight Graphite", (0.012, 0.022, 0.04), 0.45, 0.28)
accent = mat("Anodized Blue", (0.08, 0.28, 0.48), 0.8, 0.2)

# Hexagonal extruded frame
bpy.ops.mesh.primitive_cylinder_add(vertices=6, radius=2.15, depth=0.34,
                                    location=(0, 0.65, 0.0), rotation=(0,0,math.radians(30)))
frame = bpy.context.object
frame.name = "TAVIQ_3D_Hexagon"
frame.data.materials.append(silver)
bev = frame.modifiers.new("Precision bevel", "BEVEL")
bev.width = 0.10
bev.segments = 4
frame.modifiers.new("Weighted corner normals", "WEIGHTED_NORMAL")

# T-shaped solid mark, built from bevelled bars
def bar(name, loc, scale, material):
    bpy.ops.mesh.primitive_cube_add(size=1, location=loc)
    o = bpy.context.object
    o.name = name
    o.dimensions = scale
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    o.data.materials.append(material)
    b = o.modifiers.new("Machined edges", "BEVEL")
    b.width = 0.07
    b.segments = 4
    o.modifiers.new("Weighted normals", "WEIGHTED_NORMAL")
    return o

bar("TAVIQ_Top_Bar", (0, 1.0, 0.28), (2.25, 0.34, 0.30), accent)
bar("TAVIQ_Stem", (0, 0.12, 0.28), (0.48, 1.55, 0.30), silver)

# 3D text wordmark
bpy.ops.object.text_add(location=(-2.65, -2.55, 0.0), rotation=(0,0,0))
txt = bpy.context.object
txt.name = "TAVIQ_3D_Wordmark"
txt.data.body = "TAVIQ"
txt.data.align_x = 'CENTER'
txt.data.size = 1.05
txt.data.extrude = 0.08
txt.data.bevel_depth = 0.018
txt.data.bevel_resolution = 3
txt.data.materials.append(silver)

bpy.ops.object.text_add(location=(-2.1, -3.15, 0.0))
sub = bpy.context.object
sub.name = "TAVIQ_Tagline"
sub.data.body = "BEYOND CODE  /  BEYOND LIMITS"
sub.data.align_x = 'CENTER'
sub.data.size = 0.22
sub.data.extrude = 0.015
sub.data.materials.append(accent)

# Ground plane
bpy.ops.mesh.primitive_plane_add(size=200, location=(0,0,-0.55))
plane = bpy.context.object
plane.name = "Studio_Backdrop"
plane.data.materials.append(dark)

# Lighting
def area(name, loc, energy, color, size):
    bpy.ops.object.light_add(type='AREA', location=loc)
    l = bpy.context.object
    l.name = name
    l.data.energy = energy
    l.data.color = color
    l.data.shape = 'DISK'
    l.data.size = size
    direction = Vector((0,0,0)) - l.location
    l.rotation_euler = direction.to_track_quat('-Z','Y').to_euler()

area("Key_Softbox", (1,4,7), 1100, (0.72,0.86,1.0), 5)
area("Blue_Rim", (-5,1,3), 850, (0.12,0.42,1.0), 4)
area("Warm_Fill", (4,-3,4), 500, (0.55,0.75,1.0), 3)

bpy.ops.object.camera_add(location=(0,-1.7,10))
cam = bpy.context.object
cam.rotation_euler = (math.radians(8),0,0)
cam.data.type = 'ORTHO'
cam.data.ortho_scale = 8.2
bpy.context.scene.camera = cam

scene = bpy.context.scene
scene.render.engine = 'BLENDER_EEVEE_NEXT' if hasattr(bpy.types, "EEVEE_NEXT") else 'BLENDER_EEVEE'
scene.render.resolution_x = 1400
scene.render.resolution_y = 1400
scene.render.resolution_percentage = 100
scene.render.image_settings.file_format = 'PNG'
scene.render.filepath = "//taviq-3d-logo.png"
scene.world.color = (0.018,0.025,0.04)
scene.view_settings.view_transform = 'AgX'
bpy.ops.wm.save_as_mainfile(filepath="//taviq-logo-source.blend")
bpy.ops.render.render(write_still=True)
