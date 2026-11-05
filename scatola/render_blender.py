# Render della scatola con Blender (senza aprire la finestra):
#   /Applications/Blender.app/Contents/MacOS/Blender -b -P scatola/render_blender.py -- [colore]
# colore: "nero" (predefinito) o "bianco". Prima esportare i pezzi in posa:
#   openscad -D 'part="cornice"' -D posa=true -o scatola/prove/posa/cornice.stl scatola/scatola.scad
#   (lo stesso per retro, scheda, supporto)
# Le misure arrivano in mm da OpenSCAD: qui diventano metri (x 0,001).

import math
import os
import sys

import bpy
from mathutils import Matrix, Vector

HERE = os.path.dirname(os.path.abspath(__file__))
POSA = os.path.join(HERE, "prove", "posa")
OUT = os.path.join(HERE, "render")
SCREEN = os.path.join(HERE, "..", "design", "home_giorni", "schermate", "21_finale.png")

args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
COLORE = args[0] if args else "nero"
SCELTE = args[1:]          # numeri delle inquadrature da fare (vuoto = tutte)

# Stessi numeri di scatola.scad
TILT = math.radians(25)
SHELF_H = 36
TOP_Z = 14.9
GLASS_Z = 13.0
AA = (14.36, 11.39, 108.00, 64.80)
PCB_W = 136.73
UART_X = 50.69
OY = -2.4
PORT_Z = 5.0
ST_T, ST_GAP, ST_W = 4.0, 0.4, 116
BOSS_D, ROD_D, ROD_L = 20, 12, 112
TABLE_Z = -1.0             # la base del supporto scende a z -1 (piedini esclusi)
SLAB_M = Matrix(((1, 0, 0, -PCB_W / 2),
                 (0, math.sin(TILT), -math.cos(TILT), -OY * math.sin(TILT) + TOP_Z * math.cos(TILT)),
                 (0, math.cos(TILT), math.sin(TILT), SHELF_H - OY * math.cos(TILT) - TOP_Z * math.sin(TILT)),
                 (0, 0, 0, 1)))
FRONT = Vector((0, -math.cos(TILT), math.sin(TILT)))   # direzione in cui guarda lo schermo
MM = 0.001

bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene


def material(name, color, rough, metal=0.0, coat=0.0):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    b = m.node_tree.nodes["Principled BSDF"]
    b.inputs["Base Color"].default_value = (*color, 1)
    b.inputs["Roughness"].default_value = rough
    b.inputs["Metallic"].default_value = metal
    b.inputs["Coat Weight"].default_value = coat
    b.inputs["Specular IOR Level"].default_value = 0.3   # plastica stampata, opaca
    return m


if COLORE == "bianco":
    case_col, stand_col = (0.78, 0.78, 0.76), (0.78, 0.78, 0.76)
else:
    case_col, stand_col = (0.025, 0.025, 0.027), (0.035, 0.035, 0.038)

MAT = {
    "cornice": material("pla_cornice", case_col, 0.62),
    "retro": material("pla_retro", case_col, 0.62),
    "supporto": material("pla_supporto", stand_col, 0.62),
    "scheda": material("scheda", (0.01, 0.01, 0.01), 0.25, coat=0.6),
}


def load(name):
    bpy.ops.wm.stl_import(filepath=os.path.join(POSA, name + ".stl"), global_scale=MM)
    ob = bpy.context.selected_objects[0]
    ob.name = name
    ob.data.materials.append(MAT[name])
    for p in ob.data.polygons:
        p.use_smooth = False
    # spigoli morbidi senza arrotondare le facce piane
    mod = ob.modifiers.new("smussi", "BEVEL")
    mod.width = 0.00025
    mod.segments = 2
    mod.limit_method = "ANGLE"
    mod.angle_limit = math.radians(35)
    return ob


parts = {n: load(n) for n in ("supporto", "cornice", "retro", "scheda")}

# Schermo: rettangolo dell'area attiva sul vetro con la schermata vera della Home
corners = [(AA[0], AA[1]), (AA[0] + AA[2], AA[1]), (AA[0] + AA[2], AA[1] + AA[3]), (AA[0], AA[1] + AA[3])]
verts = [(SLAB_M @ Vector((x, y, GLASS_Z + 0.03))) * MM for x, y in corners]
me = bpy.data.meshes.new("schermo")
me.from_pydata(verts, [], [(0, 1, 2, 3)])
uv = me.uv_layers.new()
for loop, coord in zip(me.loops, [(0, 0), (1, 0), (1, 1), (0, 1)]):
    uv.data[loop.index].uv = coord
screen = bpy.data.objects.new("schermo", me)
scene.collection.objects.link(screen)
sm = bpy.data.materials.new("schermo")
sm.use_nodes = True
nt = sm.node_tree
b = nt.nodes["Principled BSDF"]
img = nt.nodes.new("ShaderNodeTexImage")
img.image = bpy.data.images.load(os.path.abspath(SCREEN))
nt.links.new(img.outputs["Color"], b.inputs["Emission Color"])
b.inputs["Emission Strength"].default_value = 2.2
b.inputs["Base Color"].default_value = (0, 0, 0, 1)
b.inputs["Roughness"].default_value = 0.12
b.inputs["Coat Weight"].default_value = 0.35
b.inputs["Coat Roughness"].default_value = 0.08
screen.data.materials.append(sm)


def local_to_world(p):
    """Punto in mm nelle coordinate della scatola -> metri sul supporto."""
    return (SLAB_M @ Vector(p)) * MM


# Spina USB-C nella presa UART e cavo che scende nel supporto ed esce dietro
def box(name, lo, hi, mat, bevel):
    xs, ys, zs = (lo[0], hi[0]), (lo[1], hi[1]), (lo[2], hi[2])
    verts = [local_to_world((x, y, z)) for x in xs for y in ys for z in zs]
    faces = [(0, 1, 3, 2), (4, 6, 7, 5), (0, 4, 5, 1), (2, 3, 7, 6), (0, 2, 6, 4), (1, 5, 7, 3)]
    me = bpy.data.meshes.new(name)
    me.from_pydata(verts, [], faces)
    me.update()
    ob = bpy.data.objects.new(name, me)
    scene.collection.objects.link(ob)
    ob.data.materials.append(mat)
    mod = ob.modifiers.new("tondo", "BEVEL")
    mod.width = bevel * MM
    mod.segments = 5
    return ob


# Cavo consigliato dalla SPEC_SCATOLA: nero, spina a 90 gradi "su/giu'" girata verso il retro
cm = material("cavo", (0.02, 0.02, 0.022), 0.4)
plug = [
    # corpo nella presa, gomito che piega verso il retro (z locale negativo), passacavo
    box("spina", (UART_X - 5.3, -10.5, PORT_Z - 2.7), (UART_X + 5.3, -0.5, PORT_Z + 2.7), cm, 1.8),
    box("gomito", (UART_X - 4.6, -10.5, -5.0), (UART_X + 4.6, -4.5, PORT_Z - 1.0), cm, 1.8),
    box("passacavo", (UART_X - 2.9, -9.3, -9.5), (UART_X + 2.9, -5.7, -4.5), cm, 1.2),
]
start = local_to_world((UART_X, -7.5, -9.0)) / MM
back = Vector((0, math.cos(TILT), -math.sin(TILT)))      # z locale negativo nel mondo
cz = TABLE_Z + 1.8                                       # cavo appoggiato sul tavolo
base_top = ST_T / 2 + (ST_T + 2) / 2 + 1.8               # cavo appoggiato sulla base
path = [start, start + back * 5] + [Vector((start.x, y, z)) for y, z in (
    (start.y + 10, start.z - 9), (start.y + 16, base_top), (54, base_top), (64, cz + 1.6), (78, cz), (115, cz))]
path.append(Vector((-40, 170, cz)))
cu = bpy.data.curves.new("cavo", "CURVE")
cu.dimensions = "3D"
cu.bevel_depth = 1.8 * MM
cu.bevel_resolution = 4
cu.use_fill_caps = True
sp = cu.splines.new("BEZIER")
sp.bezier_points.add(len(path) - 1)
for bp, p in zip(sp.bezier_points, path):
    bp.co = p * MM
    bp.handle_left_type = bp.handle_right_type = "AUTO"
cable = bpy.data.objects.new("cavo", cu)
scene.collection.objects.link(cable)
cable.data.materials.append(cm)
plug.append(cable)

# Tondino d'acciaio di zavorra nel rinforzo davanti (stessi conti di supporto() in scatola.scad)
t = math.tan(TILT)
c = ST_GAP + ST_T / 2
lip_bot = (-c * math.cos(TILT) - c * math.sin(TILT), SHELF_H + c * math.sin(TILT) - c * math.cos(TILT))
front_u = lip_bot[0] - (lip_bot[1] - ST_T / 2) * t
zb = BOSS_D / 2 - 1
boss_u = front_u + (zb - ST_T / 2) * t + (BOSS_D / 2 - ST_T / 2) / math.cos(TILT)
bpy.ops.mesh.primitive_cylinder_add(vertices=64, radius=ROD_D / 2 * MM, depth=ROD_L * MM,
                                    location=(0, boss_u * MM, zb * MM), rotation=(0, math.radians(90), 0))
rod = bpy.context.object
rod.name = "tondino"
rod.data.materials.append(material("acciaio", (0.55, 0.55, 0.56), 0.35, metal=1.0))
for f in rod.data.polygons:
    f.use_smooth = True


def show_cable(on):
    for ob in plug:
        ob.hide_render = not on


# Tavolo e fondale
bpy.ops.mesh.primitive_plane_add(size=3, location=(0, 0.4, TABLE_Z * MM))
floor = bpy.context.object
fm = material("tavolo", (0.30, 0.28, 0.26), 0.8)
floor.data.materials.append(fm)
bpy.ops.mesh.primitive_plane_add(size=3, location=(0, 1.1, 1.0), rotation=(math.radians(90), 0, 0))
backdrop = bpy.context.object
backdrop.data.materials.append(fm)

world = bpy.data.worlds.new("mondo")
world.use_nodes = True
bg = world.node_tree.nodes["Background"]
scene.world = world


def studio(on):
    """on: senza tavolo, scatola sospesa su un fondo uniforme (viste da dietro e da sotto)."""
    floor.hide_render = backdrop.hide_render = on
    bg.inputs["Color"].default_value = (0.42, 0.42, 0.44, 1) if on else (0.55, 0.56, 0.6, 1)
    bg.inputs["Strength"].default_value = 0.55 if on else 0.15


studio(False)


def area_light(name, loc, energy, size, target=(0, 0.02, 0.07)):
    d = bpy.data.lights.new(name, "AREA")
    d.energy = energy
    d.size = size
    ob = bpy.data.objects.new(name, d)
    ob.location = loc
    scene.collection.objects.link(ob)
    dirv = Vector(target) - Vector(loc)
    ob.rotation_euler = dirv.to_track_quat("-Z", "Y").to_euler()
    return d


area_light("chiave", (-0.45, -0.5, 0.55), 22, 0.5)
area_light("riempimento", (0.6, -0.35, 0.25), 6, 0.6)
area_light("controluce", (0.2, 0.6, 0.5), 18, 0.4)
low_light = area_light("da_sotto", (0.25, -0.30, -0.35), 0, 0.6)   # accesa solo nella vista da sotto

target = bpy.data.objects.new("mira", None)
scene.collection.objects.link(target)
cam_data = bpy.data.cameras.new("camera")
cam = bpy.data.objects.new("camera", cam_data)
scene.collection.objects.link(cam)
scene.camera = cam
tc = cam.constraints.new("TRACK_TO")
tc.target = target
tc.track_axis = "TRACK_NEGATIVE_Z"
tc.up_axis = "UP_Y"

scene.render.engine = "CYCLES"
RAPIDO = "RAPIDO" in os.environ
scene.cycles.samples = 16 if RAPIDO else 160
scene.cycles.use_denoising = True
prefs = bpy.context.preferences.addons["cycles"].preferences
try:
    prefs.compute_device_type = "METAL"
    prefs.get_devices()
    for d in prefs.devices:
        d.use = True
    scene.cycles.device = "GPU"
except Exception:
    pass
scene.view_settings.view_transform = "AgX"
scene.view_settings.look = "AgX - Medium High Contrast"
scene.view_settings.exposure = -1.3
scene.render.resolution_x = 800 if RAPIDO else 1600
scene.render.resolution_y = 550 if RAPIDO else 1100
scene.render.film_transparent = False
scene.render.image_settings.file_format = "JPEG"   # PNG da 2 MB l'uno: troppo per il repository
scene.render.image_settings.quality = 88

os.makedirs(OUT, exist_ok=True)


def shot(name, cam_loc, aim, lens=70):
    if SCELTE and name.split("_")[0] not in SCELTE:
        return
    cam.location = Vector(cam_loc)
    target.location = Vector(aim)
    cam_data.lens = lens
    scene.render.filepath = os.path.join(OUT, f"{name}_{COLORE}.jpg")
    bpy.ops.render.render(write_still=True)
    print("RENDER", scene.render.filepath)
    if RAPIDO:
        raise SystemExit


show_cable(False)
shot("1_fronte", (-0.24, -0.40, 0.20), (0.0, 0.03, 0.055), 55)
shot("2_retro", (0.30, 0.36, 0.20), (0.0, 0.03, 0.05), 55)
shot("3_lato", (-0.42, -0.06, 0.10), (0.0, 0.03, 0.06), 70)
# Scritte: bordo superiore visto dall'alto dal davanti, retro del supporto da vicino
shot("9_scritta_bordo", (-0.07, -0.13, 0.31), (0.0, 0.044, 0.112), 75)
shot("10_scritta_supporto", (0.10, 0.30, 0.16), (0.0, 0.06, 0.04), 70)

# Con il cavo nella presa UART: da dietro in basso (uscita dalla base) e di lato (percorso nel supporto)
show_cable(True)
shot("7_cavo_dietro", (0.29, 0.38, 0.11), (-0.01, 0.03, 0.05), 50)
shot("8_cavo_lato", (-0.40, 0.0, 0.06), (0.0, 0.022, 0.045), 50)

# Senza supporto: scatola chiusa vista da dietro e dal bordo con le prese
parts["supporto"].hide_render = rod.hide_render = True
show_cable(False)
studio(True)
center = local_to_world((68.37, 41.85, 7.45))
shot("5_retro_scatola", center + Vector((0.17, 0.30, 0.16)), center, 60)
low_light.energy = 14
shot("6_sotto_scatola", center + Vector((0.12, -0.20, -0.27)), center + Vector((0, 0, -0.012)), 60)
low_light.energy = 0
studio(False)

# Esploso: cornice e retro staccati lungo la direzione dello schermo
for n, dist in (("cornice", 45), ("retro", -40)):
    parts[n].location = FRONT * dist * MM
shot("4_esploso", (-0.30, -0.36, 0.30), (0.0, 0.0, 0.08), 55)
