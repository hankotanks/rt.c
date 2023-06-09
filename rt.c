#include<string.h>

#include "geom.h"
#include "buffer.h"
#include "in.h"
#include "intrs.h"
#include "scene.h"

//
// Raytracing

Ray camera_ray(Scene s, size_t h, size_t w, size_t x, size_t y) {
    Vec camera_dir = norm_v(sub_vv(s.camera.at, s.camera.pos));

    Vec up = vec_abc(0., -1., 0.);
    Vec right = cross_vv(camera_dir, up);

    double norm_x = ((double) x / (double) w) - 0.5;
    double norm_y = ((double) y / (double) h) - 0.5;

    Vec i = mul_vs(right, norm_x);
    Vec j = mul_vs(up, norm_y);

    Vec img_pt = add_vv(add_vv(add_vv(i, j), s.camera.pos), camera_dir);

    return (Ray) {
        .origin = s.camera.pos,
        .dir = norm_v(sub_vv(img_pt, s.camera.pos))
    };
}

Vec cast(Scene s, Config c, size_t h, size_t w, size_t x, size_t y) {
    Ray r = camera_ray(s, h, w, x, y);

    Intersection intrs = intersection_check(s, c, r);
    if(!intrs.s.st) return vec_aaa(0.);

    Vec normal, hit;
    intersection_normal(intrs, r, &normal, &hit);

    Material* material = intersection_material(intrs);

    Vec pixel_color = mul_vs(material->color_ambient, c.ambience);
    Light* l = s.lights; while(l) {
        Ray light_ray = (Ray) {
            .origin = hit,
            .dir = norm_v(sub_vv(l->pos, hit))
        };
        
        Intersection shadow = intersection_check_excl(s, c, light_ray, intrs.s);
        if(!shadow.s.st) {
            double diffuse = MAX(0., dot_vv(normal, light_ray.dir) * l->strength);

            pixel_color = add_vv(pixel_color, mul_vs(material->color_diffuse, diffuse));

            Vec refl = sub_vv(r.dir, mul_vs(normal, 2. * dot_vv(normal, r.dir)));

            double spec = MAX(0., material->luster * pow(dot_vv(refl, light_ray.dir), material->metallicity));

            pixel_color = add_vv(pixel_color, mul_vs(material->color_spec, spec));
        }

        l = l->next;
    }

    return clamp_v(pixel_color, 0., 1.);
}

void raytrace(Buffer b, Scene s, Config c) {
    assert(s.tt);

    size_t x, y;
    for(x = 0; x < b.w; x++)
        for(y = 0; y < b.h; y++) {
            Vec color = cast(s, c, b.h, b.w, x, y);

            buffer_set_pixel(b, x, y, color);
        }
}

//
// Main function

int main(void) {
    Camera camera = (Camera) { .pos = vec_abc(0., 10., -15.0), .at = vec_aaa(0.) };

    Scene scene = scene_new(camera);

    Material shiny_orange_temp = (Material) {
        .name = malloc(strlen("ShinyOrange") + 1),
        .color_ambient = vec_abc(1., 0.4, 0.),
        .color_diffuse = vec_abc(1., 0.4, 0.),
        .color_spec = vec_abc(1., 0.4, 0.),
        .luster = 1.,
        .metallicity = 125.
    }; strcpy(shiny_orange_temp.name, "ShinyOrange");
    Material* shiny_orange = scene_add_material(&scene, shiny_orange_temp);

    Material blue_temp = (Material) {
        .name = malloc(strlen("Blue") + 1),
        .color_ambient = vec_abc(0.2, 0.2, 1.),
        .color_diffuse = vec_abc(0.2, 0.2, 1.),
        .color_spec = vec_abc(0.2, 0.2, 1.),
        .luster = 0.5,
        .metallicity = 50.
    }; strcpy(blue_temp.name, "Blue");
    Material* blue = scene_add_material(&scene, blue_temp);

    Material muddy_green_temp = (Material) {
        .name = malloc(strlen("MuddyGreen") + 1),
        .color_ambient = vec_abc(0.2, 0.4, 0.),
        .color_diffuse = vec_abc(0.2, 0.4, 0.),
        .color_spec = vec_abc(0.2, 0.4, 0.),
        .luster = 1.,
        .metallicity = 75.,
    }; strcpy(muddy_green_temp.name, "MuddyGreen");
    Material* muddy_green = scene_add_material(&scene, muddy_green_temp);

    scene_add_mesh(&scene, mesh_from_raw("teapot", "./models/uteapot", shiny_orange));

    scene_add_light(&scene, light_new(vec_abc(15., 10., 0.), 0.8));
    scene_add_light(&scene, light_new(vec_abc(-15., 10., 0.), 0.8));

    scene_add_sphere(&scene, (Sphere) { 
        .center = vec_abc(0.0, 0.0, 15.0), 
        .radius = 10.,
        .material = blue 
    } );

    scene_add_sphere(&scene, (Sphere) {
       .center = vec_abc(8., -8., 6.),
       .radius = 4.,
       .material = muddy_green
    } );

    scene_initialize(&scene);

    Config c = (Config) {
        .t_min = 0.01,
        .t_max = 1000.,
        .fov = 1.570796,
        .ambience = 0.2
    };

    Buffer b = buffer_wh(640, 360);

    raytrace(b, scene, c);

    buffer_export_as_ppm(b, "test.ppm");

    printf("Complete...\n");

    scene_free(&scene);

    buffer_free(&b);

    return 0;
}

/*
    NEXT STEPS:
    + finalize an API
    + implement API & associated new features (Mesh as SLL, etc)
    - Write parser for MTL and OBJ files
    - Mesh creation should specify a material name to use
    - Add a list of Material objects to the Scene
        - Should it be a member of Scene or Config?
    + Should Spheres and Lights also be SLL?
        + Or should Spheres occupy the BVH
*/

/*
    NEXT STEPS:
    - Add `Material*` member to Sphere & Tri
    - Add `Material* mats` to Scene
 */

/*

int main(void) {
    Camera camera = camera_new(vec_abc(0., 0., -10.), vec_aaa(0.));

    Scene scene = scene_new(camera);
    scene_add_materials("./models/materials.mtl");
    scene_add_mesh(scene, mesh_from_raw("teapot", "./models/uteapot", "ShinyOrange"));
    
    Mesh* teapot = scene_get_mesh(scene, "teapot");
    mesh_transform(teapot, TRANSLATE, vec_abc(4., 4., 0.));
    
    scene_initialize(scene);

    Config config = (Config) {
        .t_min = 0.01,
        .t_max = 1000.,
        .fov = 1.570796,
        .ambience = 0.2
    };

    Buffer buffer = buffer_wh(640, 360);

    raytrace(buffer, scene, config);

    buffer_export_as_ppm(b, "test.ppm");
}

*/