#include "render.h"
#include "texture.h"
#include <cmath>
#include <fmt/core.h>
#include <glm/geometric.hpp>
#include <glm/gtx/string_cast.hpp>
#include <shading.h>

// This function is provided as-is. You do not have to implement it (unless
// you need to for some extra feature).
// Given render state and an intersection, based on render settings, sample
// the underlying material data in the expected manner.
glm::vec3 sampleMaterialKd(RenderState& state, const HitInfo& hitInfo)
{
    if (state.features.enableTextureMapping && hitInfo.material.kdTexture) {
        if (state.features.enableBilinearTextureFiltering) {
            return sampleTextureBilinear(*hitInfo.material.kdTexture, hitInfo.texCoord);
        } else {
            return sampleTextureNearest(*hitInfo.material.kdTexture, hitInfo.texCoord);
        }
    } else {
        return hitInfo.material.kd;
    }
}

// This function is provided as-is. You do not have to implement it.
// Given a camera direction, a light direction, a relevant intersection, and a color coming in
// from the light, evaluate the scene-selected shading model, returning the reflected light towards the target.
glm::vec3 computeShading(RenderState& state, const glm::vec3& cameraDirection, const glm::vec3& lightDirection, const glm::vec3& lightColor, const HitInfo& hitInfo)
{
    // Hardcoded linear gradient. Feel free to modify this
    static LinearGradient gradient = {
        .components = {
            { 0.1f, glm::vec3(215.f / 256.f, 210.f / 256.f, 203.f / 256.f) },
            { 0.22f, glm::vec3(250.f / 256.f, 250.f / 256.f, 240.f / 256.f) },
            { 0.5f, glm::vec3(145.f / 256.f, 170.f / 256.f, 175.f / 256.f) },
            { 0.78f, glm::vec3(255.f / 256.f, 250.f / 256.f, 205.f / 256.f) },
            { 0.9f, glm::vec3(170.f / 256.f, 170.f / 256.f, 170.f / 256.f) },
        }
    };

    if (state.features.enableShading) {
        switch (state.features.shadingModel) {
            case ShadingModel::Lambertian:
                return computeLambertianModel(state, cameraDirection, lightDirection, lightColor, hitInfo);
            case ShadingModel::Phong:
                return computePhongModel(state, cameraDirection, lightDirection, lightColor, hitInfo);
            case ShadingModel::BlinnPhong:
                return computeBlinnPhongModel(state, cameraDirection, lightDirection, lightColor, hitInfo);
            case ShadingModel::LinearGradient:
                return computeLinearGradientModel(state, cameraDirection, lightDirection, lightColor, hitInfo, gradient);
        };
    }

    return lightColor * sampleMaterialKd(state, hitInfo);
}

// Given a camera direction, a light direction, a relevant intersection, and a color coming in
// from the light, evaluate a Lambertian diffuse shading, returning the reflected light towards the target.
glm::vec3 computeLambertianModel(RenderState& state, const glm::vec3& cameraDirection, const glm::vec3& lightDirection, const glm::vec3& lightColor, const HitInfo& hitInfo)
{
    // Implement basic diffuse shading if you wish to use it
    auto d_prod = dot(hitInfo.normal, normalize(lightDirection));
    auto temp = sampleMaterialKd(state, hitInfo) * glm::max(d_prod, 0.0f);

    auto D = temp * lightColor;

    return D;
}

// TODO: Standard feature
// Given a camera direction, a light direction, a relevant intersection, and a color coming in
// from the light, evaluate the Phong Model returning the reflected light towards the target.
// Note: materials do not have an ambient component, so you can ignore this.
// Note: use `sampleMaterialKd` instead of material.kd to automatically forward to texture
//       sampling if a material texture is available!
//
// - state;           the active scene, feature config, and the bvh
// - cameraDirection; exitant vector towards the camera (or secondary position)
// - lightDirection;  exitant vector towards the light
// - lightColor;      the color of light along the lightDirection vector
// - hitInfo;         hit object describing the intersection point
// - return;          the result of shading along the cameraDirection vector
//
// This method is unit-tested, so do not change the function signature.
glm::vec3 computePhongModel(RenderState& state, const glm::vec3& cameraDirection, const glm::vec3& lightDirection, const glm::vec3& lightColor, const HitInfo& hitInfo)
{
    // diffuse
    auto D = computeLambertianModel(state, cameraDirection, lightDirection, lightColor, hitInfo);
    // specular
    if (D == glm::vec3(0))
        return glm::vec3(0);
    auto L = normalize(lightDirection);
    auto reflected_light = normalize(2.0f * dot(hitInfo.normal, L) * hitInfo.normal - L);
    auto cos_phi = dot(reflected_light, normalize(cameraDirection));
    auto S = lightColor * hitInfo.material.ks * pow(glm::max(cos_phi, 0.0f), hitInfo.material.shininess);

    return S + D;
}

// TODO: Standard feature
// Given a camera direction, a light direction, a relevant intersection, and a color coming in
// from the light, evaluate the Blinn-Phong Model returning the reflected light towards the target.
// Note: materials do not have an ambient component, so you can ignore this.
// Note: use `sampleMaterialKd` instead of material.kd to automatically forward to texture
//       sampling if a material texture is available!
//
// - state;           the active scene, feature config, and the bvh
// - cameraDirection; exitant vector towards the camera (or secondary position)
// - lightDirection;  exitant vector towards the light
// - lightColor;      the color of light along the lightDirection vector
// - hitInfo;         hit object describing the intersection point
// - return;          the result of shading along the cameraDirection vector
//
// This method is unit-tested, so do not change the function signature.
glm::vec3 computeBlinnPhongModel(RenderState& state, const glm::vec3& cameraDirection, const glm::vec3& lightDirection, const glm::vec3& lightColor, const HitInfo& hitInfo)
{
    auto D = computeLambertianModel(state, cameraDirection, lightDirection, lightColor, hitInfo);
    if (D == glm::vec3(0))
        return glm::vec3(0);

    auto H = normalize(normalize(cameraDirection) + normalize(lightDirection));
    float temp = dot(H, hitInfo.normal);

    auto S = lightColor * hitInfo.material.ks * pow(glm::max(temp, 0.0f), hitInfo.material.shininess);
    return S + D;
}

// TODO: Standard feature
// Given a number ti between [-1, 1], sample from the gradient's components and return the
// linearly interpolated color, for which ti lies in the interval between the t-values of two
// components, or on a boundary. If ti falls outside the gradient's smallest/largest components,
// the nearest component must be sampled.
// - ti; a number between [-1, 1]
// This method is unit-tested, so do not change the function signature.
glm::vec3 LinearGradient::sample(float ti) const
{
    auto sortedComponents = components;
    int j;
    //sort
    for (int i = components.size() - 1; i >= 0; i--) {
        j = i - 1;
        while(j >= 0 && components.at(j).t > components.at(i).t) {
            sortedComponents.at(j + 1) = sortedComponents.at(j);
            j--;
        }
        sortedComponents.at(j + 1) = components.at(i);
    }

    /*float max = components.at(0).t, min = components.at(0).t;
    Component maxComponent = components.at(0), minComponent = components.at(0);

    for (auto component : components) {
        if (ti = component.t)
                return component.color;
        if (component.t > max) {
            max = component.t;
            maxComponent = component;
        } else if (component.t < min) {
            min = component.t;
            minComponent = component;
        }
    }*/

    if (ti <= sortedComponents.at(0).t)
        return sortedComponents.at(0).color;
    else if (ti >= sortedComponents.at(sortedComponents.size() - 1).t)
        return sortedComponents.at(sortedComponents.size() - 1).color;

    Component lower = sortedComponents.at(0), upper = sortedComponents.at(0);

    for (auto component : sortedComponents) {
        if (component.t = ti)
            return component.color;
        if (component.t > ti && component.t < upper.t)
            upper = component;
        else if (component.t < ti)
            lower = component;
    }

    auto coefLower = (ti - lower.t) / (upper.t - lower.t);
    auto coefUpper = 1.0f - coefLower;

    return coefLower * lower.color + coefUpper * upper.color;
}

// TODO: Standard feature
// Given a camera direction, a light direction, a relevant intersection, and a color coming in
// from the light, evaluate a diffuse shading model, such that the diffuse component is sampled not
// from the intersected material, but a provided linear gradient, based on the cosine of theta
// as defined in the diffuse shading part of the Phong model.
//
// - state;           the active scene, feature config, and the bvh
// - cameraDirection; exitant vector towards the camera (or secondary position)
// - lightDirection;  exitant vector towards the light
// - lightColor;      the color of light along the lightDirection vector
// - hitInfo;         hit object describing the intersection point
// - gradient;        the linear gradient object
// - return;          the result of shading
//
// This method is unit-tested, so do not change the function signature.
glm::vec3 computeLinearGradientModel(RenderState& state, const glm::vec3& cameraDirection, const glm::vec3& lightDirection, const glm::vec3& lightColor, const HitInfo& hitInfo, const LinearGradient& gradient)
{
    float cos_theta = glm::dot(lightDirection, hitInfo.normal);
    auto D = gradient.sample(glm::max(cos_theta, 0.0f)) * lightColor;

    return D;
}