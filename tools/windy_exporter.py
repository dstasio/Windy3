import bpy
import struct

VERSION = 2
ISDEBUG = False

bl_info = {
    "name": "Windy Exporter",
    "blender": (2, 80, 0),
    "category": "Import-Export",
}

def float_to_int(f):
    return struct.unpack('i', struct.pack('f', f))[0]

class WexpExport(bpy.types.Operator):
    """Test exporter which just writes hello world"""
    bl_idname = "export.wexp"
    bl_label = "Wexp"

    filepath: bpy.props.StringProperty(subtype="FILE_PATH")
    settings_triangulate = True
    settings_smooth_normals = False

    @classmethod
    def poll(cls, context):
        return context.object is not None

    #
    # Currently exports:
    #  - pos/normal/uv data
    #  - triangulated indices
    #
    #  - smooth normals
    #  - only first mesh in .blend
    #  - only supports quads
    def execute(self, context):
        if(ISDEBUG):
            print('\nExporting mesh:')
        
        with open(self.filepath, 'wb') as file:
            # getting mesh indices among selected objects
            mesh_indices = []
            for object_index in range(len(bpy.context.selected_objects)):
                if bpy.context.selected_objects[object_index].type == 'MESH':
                    mesh_indices.append(object_index)
                        
            if len(bpy.context.selected_objects) == 0:
                print("WARNING: no objects selected")
            elif len(mesh_indices) == 0:
                print("WARNING: no mesh objects were selected")
            
            # Writing file header
            file.write(struct.pack('<2sBH', b'Wx', VERSION, len(mesh_indices)))
            
            for mesh_index in mesh_indices:
                object = bpy.context.selected_objects[mesh_index]
                mesh = bpy.context.selected_objects[mesh_index].data
                mesh_name = object.name
        
                #
                # Getting vertex data without repetitions
                #
                hashes = []
                ngon_indices = []
                trigon_indices = []
                vertex_data = []
                
                if self.settings_smooth_normals:
                    pass
                    # @todo: this code doesn't discriminate between tris, quads and n-gons
                    
                    #loop_indices = [mesh.polygons[x].loop_indices[y] for x in range(len(mesh.polygons)) for y in range(len(mesh.polygons[x].loop_indices))]
                    #for loop_index in loop_indices:
                    #    v = mesh.vertices[mesh.loops[loop_index].vertex_index]
                    #    uv = mesh.uv_layers.active.data[loop_index].uv
                    #    n = v.normal
                    #    v_hash  = float_to_int(v.co.x*3) + float_to_int(v.co.y*5) + float_to_int(v.co.z*7)
                    #    n_hash  = float_to_int(  n.x*11) + float_to_int(  n.y*13) + float_to_int(  n.z*17)
                    #    uv_hash = float_to_int( uv.x*19) + float_to_int( uv.y*23)
                    #    hash = v_hash*27 + n_hash*29 + uv_hash*31
                    #    if (hash in hashes):
                    #        ngon_indices.append(hashes.index(hash))
                    #    else:
                    #        vertex_data.extend([v.co.x, v.co.y, v.co.z, n.x, n.y, n.z, uv.x, uv.y])
                    #        ngon_indices.append(len(hashes))
                    #        hashes.append(hash)
                    #    
                    #    if (ISDEBUG):
                    #        print("{:2d}: {: .2f}, {: .2f}, {: .2f},  {: .2f}, {: .2f}, {: .2f},  {:.2f}, {:.2f} -> {: d}".\
                    #        format(ngon_indices[len(ngon_indices)-1], v.co.x, v.co.y, v.co.z, n.x, n.y, n.z, uv.x, uv.y, hash))
                else: # flat normals
                    for polygon in mesh.polygons:
                        for loop_index in polygon.loop_indices:
                            v = mesh.vertices[mesh.loops[loop_index].vertex_index]
                            uv = mesh.uv_layers.active.data[loop_index].uv
                            n = polygon.normal
                            v_hash  = float_to_int(v.co.x*3) + float_to_int(v.co.y*5) + float_to_int(v.co.z*7)
                            n_hash  = float_to_int(  n.x*11) + float_to_int(  n.y*13) + float_to_int(  n.z*17)
                            uv_hash = float_to_int( uv.x*19) + float_to_int( uv.y*23)
                            hash = v_hash*27 + n_hash*29 + uv_hash*31
                            if (hash in hashes):
                                if (len(polygon.loop_indices) > 3):
                                    ngon_indices.append(hashes.index(hash))
                                else:
                                    trigon_indices.append(hashes.index(hash))
                            else:
                                vertex_data.extend([v.co.x, v.co.y, v.co.z, n.x, n.y, n.z, uv.x, uv.y])
                                if (len(polygon.loop_indices) > 3):
                                    ngon_indices.append(len(hashes))
                                else:
                                    trigon_indices.append(len(hashes))
                                hashes.append(hash)
                        
                            if (ISDEBUG):
                                print("{:2d}: {: .2f}, {: .2f}, {: .2f},  {: .2f}, {: .2f}, {: .2f},  {:.2f}, {:.2f} -> {: d}".\
                                format(ngon_indices[len(ngon_indices)-1], v.co.x, v.co.y, v.co.z, n.x, n.y, n.z, uv.x, uv.y, hash))
                
                if(ISDEBUG):
                    print("Total unique vertices: {}".format(len(hashes)))
                #
                # Triangulating
                # @todo: check if this works with ngons
                for i in range(0, len(ngon_indices), 4):
                    trigon_indices.append(ngon_indices[i])
                    trigon_indices.append(ngon_indices[i+1])
                    trigon_indices.append(ngon_indices[i+2])
                    trigon_indices.append(ngon_indices[i])
                    trigon_indices.append(ngon_indices[i+2])
                    trigon_indices.append(ngon_indices[i+3])
                    
                if(ISDEBUG):
                    print("Total indices: {}".format(len(trigon_indices)))

                # writing mesh header
                mesh_header_format = ''.join((
                '<',       # (byte order = little endian)
                '2s',      # signature
                 'H',      # Vertex   data offset (from this header)  [2 bytes]
                 'I',      # Index    data offset (from this header)  [4 bytes]
                 'I',      # Next mesh/eof offset (from this header)  [4 bytes]
                 'I',      # Name offset
                 'B',      # Name size in bytes
                '3f',      # Mesh position (xyz)
                '3f',      # Mesh rotation (xyz)
                '3f'))    # Mesh scale    (xyz)
                
                mesh_header_size = struct.calcsize(mesh_header_format)
                vertex_bytes_offset = mesh_header_size
                vertex_bytes_count  = len(vertex_data)*4
                index_bytes_offset  = vertex_bytes_offset + vertex_bytes_count
                index_bytes_count   = len(trigon_indices)*2
                name_bytes_offset   = index_bytes_offset + index_bytes_count
                name_bytes_count    = len(mesh_name) + 1 # (includes 0 at end of string)
                pos_x               = object.location.x
                pos_y               = object.location.y
                pos_z               = object.location.z
                rot_x               = object.rotation_euler.x
                rot_y               = object.rotation_euler.y
                rot_z               = object.rotation_euler.z
                scale_x             = object.scale.x
                scale_y             = object.scale.y
                scale_z             = object.scale.z
                end_offset          = name_bytes_offset + name_bytes_count
                
                if (object.rotation_euler.order != 'XYZ'):
                    ___ERROR_ROTATION_MODE_NOT_SUPPORTED
                
                file.write(struct.pack(mesh_header_format,          \
                                       b'Wm',                       \
                                       vertex_bytes_offset,         \
                                       index_bytes_offset,          \
                                       end_offset,                  \
                                       name_bytes_offset,           \
                                       name_bytes_count,            \
                                         pos_x,   pos_y,   pos_z,   \
                                         rot_x,   rot_y,   rot_z,   \
                                       scale_x, scale_y, scale_z))
            
                for vertex_component in vertex_data:
                    bytes = struct.pack("<f", vertex_component)
                    file.write(bytes)
                for index in trigon_indices:
                    bytes = struct.pack("<H", index)
                    file.write(bytes)

                # writing mesh name
                file.write(struct.pack("<{}s".format(name_bytes_count-1), mesh_name.encode()))
                file.write(b'\0')
                
                #file.close()
                #file = None
        
        
        return {'FINISHED'}

    def invoke(self, context, event):
        context.window_manager.fileselect_add(self)
        return {'RUNNING_MODAL'}


# Only needed if you want to add into a dynamic menu
def menu_func(self, context):
    self.layout.operator_context = 'INVOKE_DEFAULT'
    self.layout.operator(WexpExport.bl_idname, text=".wexp")

def register():
    bpy.utils.register_class(WexpExport)
    bpy.types.TOPBAR_MT_file_export.append(menu_func)
    
def unregister():
    bpy.utils.unregister_class(WexpExport)
    bpy.types.TOPBAR_MT_file_export.remove(menu_func)


if __name__ == "__main__":
    register()
    bpy.types.TOPBAR_MT_file_export.remove(menu_func)
    bpy.ops.export.wexp('INVOKE_DEFAULT')

# list all loop indices
#[bpy.data.meshes[0].polygons[x].loop_indices[y] for x in range(len(bpy.data.meshes[0].polygons)) for y in range(len(bpy.data.meshes[0].polygons[x].loop_indices))]