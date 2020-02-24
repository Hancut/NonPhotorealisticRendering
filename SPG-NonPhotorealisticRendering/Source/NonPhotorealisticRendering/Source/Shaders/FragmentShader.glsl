#version 410

layout(location = 0) in vec2 texture_coord;

uniform sampler2D textureImage;
uniform ivec2 screenSize;
uniform int flipVertical;

// 0 - original
// 1 - grayscale
// 2 - blur
uniform int outputMode = 2;

// Flip texture horizontally when
vec2 textureCoord = vec2(texture_coord.x, (flipVertical != 0) ? 1 - texture_coord.y : texture_coord.y);

layout(location = 0) out vec4 out_color;

vec4 grayscale()
{
	vec4 color = texture(textureImage, textureCoord);
	float gray = 0.21 * color.r + 0.71 * color.g + 0.07 * color.b; 
	return vec4(gray, gray, gray,  0);
}

vec4 blur(int blurRadius)
{
	vec2 texelSize = 1.0f / screenSize;
	vec4 sum = vec4(0);
	for(int i = -blurRadius; i <= blurRadius; i++)
	{
		for(int j = -blurRadius; j <= blurRadius; j++)
		{
			sum += texture(textureImage, textureCoord + vec2(i, j) * texelSize);
		}
	}
		
	float samples = pow((2 * blurRadius + 1), 2);
	return sum / samples;
}

vec4 sobel(vec2 curr){
	vec2 texelSize = 1.0f / screenSize;
	vec4 dx = texture(textureImage, textureCoord + (curr + vec2(1, 1)) * texelSize) + 
				2 * texture(textureImage, textureCoord + (curr + vec2(1, 0)) * texelSize) +
				texture(textureImage, textureCoord + (curr + vec2(1, -1)) * texelSize) -
				texture(textureImage, textureCoord + (curr + vec2(-1, 1)) * texelSize) -
				2 * texture(textureImage, textureCoord + (curr + vec2(-1, 0)) * texelSize) -
				texture(textureImage, textureCoord + (curr + vec2(-1, -1)) * texelSize);

	vec4 dy = texture(textureImage, textureCoord + (curr + vec2(1, 1)) * texelSize) + 
				2 * texture(textureImage, textureCoord + (curr + vec2(0, 1)) * texelSize) +
				texture(textureImage, textureCoord + (curr + vec2(-1, 1)) * texelSize) -
				texture(textureImage, textureCoord + (curr + vec2(1, -1)) * texelSize) -
				2 * texture(textureImage, textureCoord + (curr + vec2(0, -1)) * texelSize) -
				texture(textureImage, textureCoord + (curr + vec2(-1, -1)) * texelSize);

	vec4 color = abs(dx) + abs(dy);
	float threshold = 1;
	if (color.x > threshold) {
		color.x = 1;
	} else {
		color.x = texture(textureImage, textureCoord + curr).x;
		//color.x = 0;
	}

	if (color.y > threshold) {
		color.y = 1;
	} else {
		color.y = texture(textureImage, textureCoord + curr).y;
		//color.y = 0;
	}

	if (color.z > threshold) {
		color.z = 1;
	} else {
		color.z = texture(textureImage, textureCoord + curr).z;
		//color.z = 0;
	}

	return color;

}

vec4 bin(vec4 out_c) {
	if (out_c.x == 1 || out_c.y == 1 || out_c.z == 1) {
		return out_c;
	} 
	else {
		return vec4(0);
	}
}

vec4 dilatation() {
	
	vec4 dil = bin(sobel(vec2(0,1))) + bin(sobel(vec2(1, 0))) +
				bin(sobel(vec2(-1, 0))) + bin(sobel(vec2(0,-1))) + bin(sobel(vec2(1,1))) + bin(sobel(vec2(-1, -1))) +
				bin(sobel(vec2(-1, 1))) + bin(sobel(vec2(1,-1))) + bin(sobel(vec2(0, 0)));

	if (dil.x > 1) {
		dil.x = 1;
	}
	if (dil.y > 1) {
		dil.y = 1;
	}
	if (dil.z > 1) {
		dil.z = 1;
	}

	if (dil.x > 0 || dil.y > 0 || dil.z > 0) {
		return dil;
	}
	return texture(textureImage, textureCoord);
}

vec4 rgbtohsv(vec4 text) {
	float r,g,b;
	float h = 0.0;
	float s = 0.0;
	float v = 0.0;

	r = text.x;
	g = text.y;
	b = text.z;
	float min = min( min(r, g), b );
	float max = max( max(r, g), b );
	v = max;               // v

	float delta = max - min;

	if( max != 0.0 )
	 s = delta / max;       // s
	else {
	 // r = g = b = 0       // s = 0, v is undefined
	 s = 0.0;
	 h = -1.0;
	 return vec4(h, s, v, 0);
	}
	if( r == max )
	 h = ( g - b ) / delta;     // between yellow & magenta
	else if( g == max )
	 h = 2.0 + ( b - r ) / delta;   // between cyan & yellow
	else
	 h = 4.0 + ( r - g ) / delta;   // between magenta & cyan

	h = h * 60.0;              // degrees

	if( h < 0.0 )
	 h += 360.0;

	return vec4(h, s, v, 0);
}

void main()
{
	switch (outputMode)
	{
		case 1:
		{
			out_color = grayscale();
			break;
		}

		case 2:
		{
			out_color = blur(3);
			break;
		}

		case 3:
		{
			out_color = sobel(vec2(0, 0));
			break;
		}

		case 4:
		{
			//out_color = sobel(vec2(0,0));
			out_color = dilatation();
			break;
		}
		case 5:
		{
			float color = rgbtohsv(texture(textureImage, textureCoord)).x;
			if (color == 0 && rgbtohsv(texture(textureImage, textureCoord)).y < 0.2) {
				out_color = vec4(1, 1, 1, 0);
			} else if (color >= 0 && color <= 60) {
				// red
				out_color = vec4(1, 0, 0, 0);
			} else if (color >= 61 && color <= 120) {
				// yellow
				out_color = vec4(1, 1, 0, 0);
			} else if (color >= 121 && color <= 180) {
				// green
				out_color = vec4(0, 1, 0, 0);
			} else if (color >= 181 && color <= 240) {
				// cyan
				out_color = vec4(0, 1, 1, 0);
			} else if (color >= 241 && color <= 300) {
				// blue
				out_color = vec4(0, 0, 1, 0);
			} else if (color >= 301 && color <= 360) {
				// magenta
				out_color = vec4(1, 0, 1, 0);
			}
			break;
		}
		default:
			out_color = texture(textureImage, textureCoord);
			break;
	}
}