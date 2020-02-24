#include "NonPhotorealisticRendering.h"

#include <vector>
#include <iostream>

#include <Core/Engine.h>

using namespace std;

// Order of function calling can be seen in "Source/Core/World.cpp::LoopUpdate()"
// https://github.com/UPB-Graphics/SPG-Framework/blob/master/Source/Core/World.cpp

NonPhotorealisticRendering::NonPhotorealisticRendering()
{
	outputMode = 0;
	gpuProcessing = false;
	saveScreenToImage = false;
	window->SetSize(1000, 800);
}

NonPhotorealisticRendering::~NonPhotorealisticRendering()
{
}

FrameBuffer *processed;

struct Element
{
	char rgb[3];
	int region;
};

void NonPhotorealisticRendering::Init()
{
	// Load default texture fore imagine processing 
	originalImage = TextureManager::LoadTexture(RESOURCE_PATH::TEXTURES + "Cube/giraffe.jpg", nullptr, "image", true, true);
	processedImage = TextureManager::LoadTexture(RESOURCE_PATH::TEXTURES + "Cube/giraffe.jpg", nullptr, "newImage", true, true);

	{
		Mesh* mesh = new Mesh("quad");
		mesh->LoadMesh(RESOURCE_PATH::MODELS + "Primitives", "quad.obj");
		mesh->UseMaterials(false);
		meshes[mesh->GetMeshID()] = mesh;
	}

	static std::string shaderPath = "Source/NonPhotorealisticRendering/Source/Shaders/";

	// Create a shader program for particle system
	{
		Shader *shader = new Shader("ImageProcessing");
		shader->AddShader((shaderPath + "VertexShader.glsl").c_str(), GL_VERTEX_SHADER);
		shader->AddShader((shaderPath + "FragmentShader.glsl").c_str(), GL_FRAGMENT_SHADER);

		shader->CreateAndLink();
		shaders[shader->GetName()] = shader;
	}
}

void NonPhotorealisticRendering::FrameStart()
{
}

void NonPhotorealisticRendering::Update(float deltaTimeSeconds)
{
	ClearScreen();

	auto shader = shaders["ImageProcessing"];
	shader->Use();

	if (saveScreenToImage)
	{
		window->SetSize(originalImage->GetWidth(), originalImage->GetHeight());
	}

	int flip_loc = shader->GetUniformLocation("flipVertical");
	glUniform1i(flip_loc, saveScreenToImage ? 0 : 1);

	int screenSize_loc = shader->GetUniformLocation("screenSize");
	glm::ivec2 resolution = window->GetResolution();
	glUniform2i(screenSize_loc, resolution.x, resolution.y);

	int outputMode_loc = shader->GetUniformLocation("outputMode");
	glUniform1i(outputMode_loc, outputMode);

	int gpuProcessing_loc = shader->GetUniformLocation("outputMode");
	glUniform1i(outputMode_loc, outputMode);

	int locTexture = shader->GetUniformLocation("textureImage");
	glUniform1i(locTexture, 0);
	auto textureImage = (gpuProcessing == true) ? originalImage : processedImage;
	textureImage->BindToTextureUnit(GL_TEXTURE0);

	RenderMesh(meshes["quad"], shader, glm::mat4(1));

	if (saveScreenToImage)
	{
		saveScreenToImage = false;
		GLenum format = GL_RGB;
		if (originalImage->GetNrChannels() == 4)
		{
			format = GL_RGBA;
		}
		glReadPixels(0, 0, originalImage->GetWidth(), originalImage->GetHeight(), format, GL_UNSIGNED_BYTE, processedImage->GetImageData());
		processedImage->UploadNewData(processedImage->GetImageData());
		SaveImage("shader_processing_" + std::to_string(outputMode));

		float aspectRatio = static_cast<float>(originalImage->GetWidth()) / originalImage->GetHeight();
		window->SetSize(static_cast<int>(600 * aspectRatio), 600);
	}
}

void NonPhotorealisticRendering::FrameEnd()
{
	DrawCoordinatSystem();
}

void NonPhotorealisticRendering::OnFileSelected(std::string fileName)
{
	if (fileName.size())
	{
		std::cout << fileName << endl;
		originalImage = TextureManager::LoadTexture(fileName.c_str(), nullptr, "image", true, true);
		processedImage = TextureManager::LoadTexture(fileName.c_str(), nullptr, "newImage", true, true);

		float aspectRatio = static_cast<float>(originalImage->GetWidth()) / originalImage->GetHeight();
		window->SetSize(static_cast<int>(600 * aspectRatio), 600);
	}
}

void NonPhotorealisticRendering::GrayScale()
{
	unsigned int channels = originalImage->GetNrChannels();
	unsigned char* data = originalImage->GetImageData();
	unsigned char* newData = processedImage->GetImageData();

	if (channels < 3)
		return;

	glm::ivec2 imageSize = glm::ivec2(originalImage->GetWidth(), originalImage->GetHeight());

	for (int i = 0; i < imageSize.y; i++)
	{
		for (int j = 0; j < imageSize.x; j++)
		{
			int offset = channels * (i * imageSize.x + j);

			// Reset save image data
			char value = static_cast<char>(data[offset + 0] * 0.2f + data[offset + 1] * 0.71f + data[offset + 2] * 0.07);
			memset(&newData[offset], value, 3);
		}
	}

	processedImage->UploadNewData(newData);
}

void NonPhotorealisticRendering::RegionGrowing()
{
	unsigned int channels = originalImage->GetNrChannels();
	unsigned char* data = originalImage->GetImageData();
	unsigned char* newData = processedImage->GetImageData();

	if (channels < 3)
		return;

	glm::ivec2 imageSize = glm::ivec2(originalImage->GetWidth(), originalImage->GetHeight());
	int** matrix = new int*[imageSize.y];
	for (int i = 0; i < imageSize.y; ++i)
		matrix[i] = new int[imageSize.x];

	int num = 0;
	int threshold = 200;
	for (int i = 0; i < imageSize.y; i++)
	{
		for (int j = 0; j < imageSize.x; j++)
		{
			int offset = channels * (i * imageSize.x + j);

			char r, g, b;

			r = static_cast<char>(data[offset + 0]);
			g = static_cast<char>(data[offset + 1]);
			b = static_cast<char>(data[offset + 2]);

			if (i - 1 >= 0 && j - 1 >= 0) {
				// has up and left neighbour
				// up neighbour data
				char r1, g1, b1;
				int offset1 = channels * ((i - 1) * imageSize.x + j);

				r1 = static_cast<char>(newData[offset1 + 0]);
				g1 = static_cast<char>(newData[offset1 + 1]);
				b1 = static_cast<char>(newData[offset1 + 2]);

				//left neighbour data
				char r2, g2, b2;
				int offset2 = channels * (i * imageSize.x + (j - 1 ));

				r2 = static_cast<char>(newData[offset2 + 0]);
				g2 = static_cast<char>(newData[offset2 + 1]);
				b2 = static_cast<char>(newData[offset2 + 2]);
				
				// diffs
				float diff1 = abs(r - r1) + abs(g - g1) + abs(b - b1);
				float diff2 = abs(r - r2) + abs(g - g2) + abs(b - b2);

				if (diff1 <= threshold || diff2 <= threshold) {
					if (diff1 < diff2) {
						matrix[i][j] = matrix[i - 1][j];
						memset(&newData[offset], r1, 1);
						memset(&newData[offset + 1], g1, 1);
						memset(&newData[offset + 2], b1, 1);
					}
					else {
						matrix[i][j] = matrix[i][j - 1];
						memset(&newData[offset], r2, 1);
						memset(&newData[offset + 1], g2, 1);
						memset(&newData[offset + 2], b2, 1);
					}
				}
				else {
					matrix[i][j] = num;
					num++;
				}
			}
			else if (i - 1 >= 0) {
				// has up neighbour
				char r1, g1, b1;
				int offset1 = channels * ((i - 1) * imageSize.x + j);

				r1 = static_cast<char>(newData[offset1 + 0]);
				g1 = static_cast<char>(newData[offset1 + 1]);
				b1 = static_cast<char>(newData[offset1 + 2]);

				float diff1 = abs(r - r1) + abs(g - g1) + abs(b - b1);

				if (diff1 <= threshold) {
					matrix[i][j] = matrix[i - 1][j];
					memset(&newData[offset], r1, 1);
					memset(&newData[offset + 1], g1, 1);
					memset(&newData[offset + 2], b1, 1);
				}
				else {
					matrix[i][j] = num;
					num++;
				}
			}
			else if (j - 1 >= 0) {
				// has left neighbour
				char r2, g2, b2;
				int offset2 = channels * (i * imageSize.x + (j - 1));

				r2 = static_cast<char>(newData[offset2 + 0]);
				g2 = static_cast<char>(newData[offset2 + 1]);
				b2 = static_cast<char>(newData[offset2 + 2]);

				float diff2 = abs(r - r2) + abs(g - g2) + abs(b - b2);

				if (diff2 <= threshold) {
					matrix[i][j] = matrix[i][j - 1];
					memset(&newData[offset], r2, 1);
					memset(&newData[offset + 1], g2, 1);
					memset(&newData[offset + 2], b2, 1);
				}
				else {
					matrix[i][j] = num;
					num++;
				}
			}
			else {
				matrix[i][j] = num;
				num++;
			}

			// Reset save image data
			/*char value = static_cast<char>(data[offset + 0] * 0.2f + data[offset + 1] * 0.71f + data[offset + 2] * 0.07);
			memset(&newData[offset], value, 3);*/
		}
	}

	processedImage->UploadNewData(newData);
}

void NonPhotorealisticRendering::SaveImage(std::string fileName)
{
	cout << "Saving image! ";
	processedImage->SaveToFile((fileName + ".png").c_str());
	cout << "[Done]" << endl;
}

// Read the documentation of the following functions in: "Source/Core/Window/InputController.h" or
// https://github.com/UPB-Graphics/SPG-Framework/blob/master/Source/Core/Window/InputController.h

void NonPhotorealisticRendering::OnInputUpdate(float deltaTime, int mods)
{
	// treat continuous update based on input
};

void NonPhotorealisticRendering::OnKeyPress(int key, int mods)
{
	// add key press event
	if (key == GLFW_KEY_F || key == GLFW_KEY_ENTER || key == GLFW_KEY_SPACE)
	{
		OpenDialogue();
	}

	if (key == GLFW_KEY_E)
	{
		gpuProcessing = !gpuProcessing;
		if (gpuProcessing == false)
		{
			outputMode = 0;
		}
		cout << "Processing on GPU: " << (gpuProcessing ? "true" : "false") << endl;
	}

	if (key == GLFW_KEY_6)
	{
		RegionGrowing();
		//GrayScale();
	}

	if (key - GLFW_KEY_0 >= 0 && key <= GLFW_KEY_5)
	{
		outputMode = key - GLFW_KEY_0;
	}

	if (key == GLFW_KEY_S && mods & GLFW_MOD_CONTROL)
	{
		if (!gpuProcessing)
		{
			SaveImage("processCPU_" + std::to_string(outputMode));
		}
		else
		{
			saveScreenToImage = true;
		}
	}
};

void NonPhotorealisticRendering::OnKeyRelease(int key, int mods)
{
	// add key release event
};

void NonPhotorealisticRendering::OnMouseMove(int mouseX, int mouseY, int deltaX, int deltaY)
{
	// add mouse move event
};

void NonPhotorealisticRendering::OnMouseBtnPress(int mouseX, int mouseY, int button, int mods)
{
	// add mouse button press event
};

void NonPhotorealisticRendering::OnMouseBtnRelease(int mouseX, int mouseY, int button, int mods)
{
	// add mouse button release event
}

void NonPhotorealisticRendering::OnMouseScroll(int mouseX, int mouseY, int offsetX, int offsetY)
{
	// treat mouse scroll event
}

void NonPhotorealisticRendering::OnWindowResize(int width, int height)
{
	// treat window resize event
}
