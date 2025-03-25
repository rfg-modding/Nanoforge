#ifndef saturate
#define saturate(v) clamp(v, 0., 1.)
#endif
vec3 hue2rgb(float hue){
    hue=fract(hue);
    return saturate(vec3(
    abs(hue*6.-3.)-1.,
    2.-abs(hue*6.-2.),
    2.-abs(hue*6.-4.)
    ));
}

//RGB to HSL (hue, saturation, lightness/luminance).
//Source: https://gist.github.com/yiwenl/745bfea7f04c456e0101
vec3 rgb2hsl(vec3 c){
    float cMin=min(min(c.r, c.g), c.b),
    cMax=max(max(c.r, c.g), c.b),
    delta=cMax-cMin;
    vec3 hsl=vec3(0., 0., (cMax+cMin)/2.);
    if (delta!=0.0){ //If it has chroma and isn't gray.
        if (hsl.z<.5){
            hsl.y=delta/(cMax+cMin);//Saturation.
        } else {
            hsl.y=delta/(2.-cMax-cMin);//Saturation.
        }
        float deltaR=(((cMax-c.r)/6.)+(delta/2.))/delta,
        deltaG=(((cMax-c.g)/6.)+(delta/2.))/delta,
        deltaB=(((cMax-c.b)/6.)+(delta/2.))/delta;
        //Hue.
        if (c.r==cMax){
            hsl.x=deltaB-deltaG;
        } else if (c.g==cMax){
            hsl.x=(1./3.)+deltaR-deltaB;
        } else { //if(c.b==cMax){
            hsl.x=(2./3.)+deltaG-deltaR;
        }
        hsl.x=fract(hsl.x);
    }
    return hsl;
}

//HSL to RGB.
//Source: https://github.com/Jam3/glsl-hsl2rgb/blob/master/index.glsl
vec3 hsl2rgb(vec3 hsl){
    if (hsl.y==0.){
        return vec3(hsl.z);//Luminance.
    } else {
        float b;
        if (hsl.z<.5){
            b=hsl.z*(1.+hsl.y);
        } else {
            b=hsl.z+hsl.y-hsl.y*hsl.z;
        }
        float a=2.*hsl.z-b;
        return a+hue2rgb(hsl.x)*(b-a);
    }
}