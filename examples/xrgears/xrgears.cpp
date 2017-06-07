/*
* Vulkan Example - Viewport array with single pass rendering using geometry shaders
*
* Copyright (C) 2017 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <vector>


#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <gli/gli.hpp>

#include <vulkan/vulkan.h>
#include "GearNode.h"
#include "vulkanexamplebase.h"
#include "VulkanModel.hpp"
#include "VulkanTexture.hpp"

#define VERTEX_BUFFER_BIND_ID 0
#define ENABLE_VALIDATION false

class VulkanExample : public VulkanExampleBase
{
public:
	// Vertex layout for the models
	vks::VertexLayout vertexLayout = vks::VertexLayout({
		vks::VERTEX_COMPONENT_POSITION,
		vks::VERTEX_COMPONENT_NORMAL,
		vks::VERTEX_COMPONENT_COLOR,
	});

	vks::Model teapotModel;

	vks::Model skyboxModel;
	vks::Texture cubeMap;

	struct {
		VkDescriptorSet object;
		VkDescriptorSet skybox;
	} descriptorSets;


	struct {
		VkPipelineVertexInputStateCreateInfo inputState;
		std::vector<VkVertexInputBindingDescription> bindingDescriptions;
		std::vector<VkVertexInputAttributeDescription> attributeDescriptions;
	} vertices;

	std::vector<VulkanGear*> gears;

	struct UBOVS {
		glm::mat4 projection;
		glm::mat4 model;
		float lodBias = 0.0f;
	} uboVS;

	struct UBOLights {
		glm::vec4 lights[4];
	} uboLights;

	struct UBOCamera {
		glm::mat4 projection[2];
		glm::mat4 view[2];
		glm::vec3 position;
	} uboCamera;

	struct {
			vks::Buffer lights;
			vks::Buffer camera;
			vks::Buffer skybox;
	} uniformBuffers;

	struct {
		VkPipeline skybox;
		//VkPipeline reflect;
	} pipelines;

	VkPipeline pipeline;
	VkPipelineLayout pipelineLayout;

		//VkPipelineLayout pipelineLayoutSky;

	VkDescriptorSet skydomeDescriptorSet;
	VkDescriptorSetLayout descriptorSetLayout;

	VkDescriptorSetLayout descriptorSetLayoutSky;

	// Camera and view properties
	float eyeSeparation = 0.08f;
	const float focalLength = 0.5f;
	const float fov = 90.0f;
	const float zNear = 0.1f;
	const float zFar = 256.0f;

	VulkanExample() : VulkanExampleBase(ENABLE_VALIDATION)
	{
		title = "XR Gears";
		enableTextOverlay = true;
		camera.type = Camera::CameraType::firstperson;
		camera.setRotation(glm::vec3(0.0f, 0.0f, 0.0f));
		//camera.setTranslation(glm::vec3(7.0f, 3.2f, 0.0f));

		camera.setTranslation(glm::vec3(2.2f, 3.2f, -7.6));

		camera.movementSpeed = 5.0f;
		timerSpeed *= 0.25f;
		//paused = true;
	}

	~VulkanExample()
	{
		vkDestroyPipeline(device, pipeline, nullptr);

		vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
		vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);

		teapotModel.destroy();

		for (auto& gear : gears)
			delete(gear);
	}

	// Enable physical device features required for this example				
	virtual void getEnabledFeatures()
	{
		// Geometry shader support is required for this example
		if (deviceFeatures.geometryShader) {
			enabledFeatures.geometryShader = VK_TRUE;
		}
		else {
			vks::tools::exitFatal("Selected GPU does not support geometry shaders!", "Feature not supported");
		}
		// Multiple viewports must be supported
		if (deviceFeatures.multiViewport) {
			enabledFeatures.multiViewport = VK_TRUE;
		}
		else {
			vks::tools::exitFatal("Selected GPU does not support multi viewports!", "Feature not supported");
		}
	}

	void buildCommandBuffers()
	{
		VkCommandBufferBeginInfo cmdBufInfo = vks::initializers::commandBufferBeginInfo();

		VkClearValue clearValues[2];
		clearValues[0].color = defaultClearColor;
		clearValues[1].depthStencil = { 1.0f, 0 };

		VkRenderPassBeginInfo renderPassBeginInfo = vks::initializers::renderPassBeginInfo();
		renderPassBeginInfo.renderPass = renderPass;
		renderPassBeginInfo.renderArea.offset.x = 0;
		renderPassBeginInfo.renderArea.offset.y = 0;
		renderPassBeginInfo.renderArea.extent.width = width;
		renderPassBeginInfo.renderArea.extent.height = height;
		renderPassBeginInfo.clearValueCount = 2;
		renderPassBeginInfo.pClearValues = clearValues;

		printf("Draw command buffers size: %d\n", drawCmdBuffers.size());

		for (int32_t i = 0; i < drawCmdBuffers.size(); ++i)
		{
			// Set target frame buffer
			renderPassBeginInfo.framebuffer = frameBuffers[i];

			VK_CHECK_RESULT(vkBeginCommandBuffer(drawCmdBuffers[i], &cmdBufInfo));

			if (vks::debugmarker::active)
				vks::debugmarker::beginRegion(drawCmdBuffers[i], "Render stuff?", glm::vec4(0.3f, 0.94f, 1.0f, 1.0f));


			vkCmdBeginRenderPass(drawCmdBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

			VkViewport viewports[2];
			// Left
			viewports[0] = { 0, 0, (float)width / 2.0f, (float)height, 0.0, 1.0f };
			// Right
			viewports[1] = { (float)width / 2.0f, 0, (float)width / 2.0f, (float)height, 0.0, 1.0f };

			vkCmdSetViewport(drawCmdBuffers[i], 0, 2, viewports);

			VkRect2D scissorRects[2] = {
				vks::initializers::rect2D(width/2, height, 0, 0),
				vks::initializers::rect2D(width/2, height, width / 2, 0),
			};
			vkCmdSetScissor(drawCmdBuffers[i], 0, 2, scissorRects);

			vkCmdSetLineWidth(drawCmdBuffers[i], 1.0f);

			//drawSkyOldPipeline(drawCmdBuffers[i]);
			drawSky(drawCmdBuffers[i]);
			//vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.skybox);
			vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
			drawTeapot(drawCmdBuffers[i]);

			for (auto& gear : gears)
				gear->draw(drawCmdBuffers[i], pipelineLayout);

			vkCmdEndRenderPass(drawCmdBuffers[i]);

			if (vks::debugmarker::active)
				vks::debugmarker::endRegion(drawCmdBuffers[i]);

			VK_CHECK_RESULT(vkEndCommandBuffer(drawCmdBuffers[i]));
		}
	}

	void drawSkyOldPipeline(VkCommandBuffer cmdbuffer) {
		VkDeviceSize offsets[1] = { 0 };

		vkCmdBindDescriptorSets(cmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &gears[0]->descriptorSet, 0, nullptr);
		vkCmdBindVertexBuffers(cmdbuffer, 0, 1, &skyboxModel.vertices.buffer, offsets);
		vkCmdBindIndexBuffer(cmdbuffer, skyboxModel.indices.buffer, 0, VK_INDEX_TYPE_UINT32);

		vkCmdPushConstants(cmdbuffer,
											 pipelineLayout,
											 VK_SHADER_STAGE_FRAGMENT_BIT,
											 sizeof(glm::vec3),
											 sizeof(Material::PushBlock), &gears[0]->material);

		vkCmdBindPipeline(cmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

		vkCmdDrawIndexed(cmdbuffer, skyboxModel.indexCount, 1, 0, 0, 0);
	}

	void drawSky(VkCommandBuffer cmdbuffer) {
		VkDeviceSize offsets[1] = { 0 };

		vkCmdBindDescriptorSets(cmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &gears[0]->descriptorSet, 0, nullptr);
		vkCmdBindVertexBuffers(cmdbuffer, 0, 1, &skyboxModel.vertices.buffer, offsets);
		vkCmdBindIndexBuffer(cmdbuffer, skyboxModel.indices.buffer, 0, VK_INDEX_TYPE_UINT32);

		vkCmdBindPipeline(cmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.skybox);

		vkCmdDrawIndexed(cmdbuffer, skyboxModel.indexCount, 1, 0, 0, 0);
	}

	void drawTeapot(VkCommandBuffer cmdbuffer) {
		vkCmdBindDescriptorSets(cmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &gears[0]->descriptorSet, 0, nullptr);
		VkDeviceSize offsets[1] = { 0 };
		vkCmdBindVertexBuffers(cmdbuffer, 0, 1, &teapotModel.vertices.buffer, offsets);
		vkCmdBindIndexBuffer(cmdbuffer, teapotModel.indices.buffer, 0, VK_INDEX_TYPE_UINT32);
		vkCmdPushConstants(cmdbuffer,
											 pipelineLayout,
											 VK_SHADER_STAGE_FRAGMENT_BIT,
											 sizeof(glm::vec3),
											 sizeof(Material::PushBlock), &gears[0]->material);
		vkCmdDrawIndexed(cmdbuffer, teapotModel.indexCount, 1, 0, 0, 0);
	}

	void loadAssets() {
		teapotModel.loadFromFile(getAssetPath() + "models/teapot.dae", vertexLayout, 0.25f, vulkanDevice, queue);
		// Skybox
		skyboxModel.loadFromFile(getAssetPath() + "models/cube.obj", vertexLayout, 10.0f, vulkanDevice, queue);
	}

	void prepareVertices()
	{
		// Gear definitions
		std::vector<float> innerRadiuses = { 1.0f, 0.5f, 1.3f };
		std::vector<float> outerRadiuses = { 4.0f, 2.0f, 2.0f };
		std::vector<float> widths = { 1.0f, 2.0f, 0.5f };
		std::vector<int32_t> toothCount = { 20, 10, 10 };
		std::vector<float> toothDepth = { 0.7f, 0.7f, 0.7f };
		std::vector<glm::vec3> colors = {
			glm::vec3(1.0f, 0.0f, 0.0f),
			glm::vec3(0.0f, 1.0f, 0.2f),
			glm::vec3(0.0f, 0.0f, 1.0f)
		};
		std::vector<glm::vec3> positions = {
			glm::vec3(-3.0, 0.0, 0.0),
			glm::vec3(3.1, 0.0, 0.0),
			glm::vec3(-3.1, -6.2, 0.0)
		};

		std::vector<Material> materials = {
			Material("Red", glm::vec3(1.0f, 0.0f, 0.0f), 1.0f, 1.0f),
			Material("Green", glm::vec3(0.0f, 1.0f, 0.2f), 0.5f, 1.0f),
			Material("Blue", glm::vec3(0.0f, 0.0f, 1.0f), 0.5f, 1.0f)
		};

		std::vector<float> rotationSpeeds = { 1.0f, -2.0f, -2.0f };
		std::vector<float> rotationOffsets = { 0.0f, -9.0f, -30.0f };

		gears.resize(positions.size());
		for (int32_t i = 0; i < gears.size(); ++i)
		{
			GearInfo gearInfo = {};
			gearInfo.innerRadius = innerRadiuses[i];
			gearInfo.outerRadius = outerRadiuses[i];
			gearInfo.width = widths[i];
			gearInfo.numTeeth = toothCount[i];
			gearInfo.toothDepth = toothDepth[i];
			gearInfo.color = colors[i];
			gearInfo.pos = positions[i];
			gearInfo.rotSpeed = rotationSpeeds[i];
			gearInfo.rotOffset = rotationOffsets[i];
			gearInfo.material = materials[i];

			gears[i] = new VulkanGear(vulkanDevice);
			gears[i]->generate(&gearInfo, queue);
		}

		// Binding and attribute descriptions are shared across all gears
		vertices.bindingDescriptions.resize(1);
		vertices.bindingDescriptions[0] =
			vks::initializers::vertexInputBindingDescription(
				VERTEX_BUFFER_BIND_ID,
				sizeof(Vertex),
				VK_VERTEX_INPUT_RATE_VERTEX);

		// Attribute descriptions
		// Describes memory layout and shader positions
		vertices.attributeDescriptions.resize(3);
		// Location 0 : Position
		vertices.attributeDescriptions[0] =
			vks::initializers::vertexInputAttributeDescription(
				VERTEX_BUFFER_BIND_ID,
				0,
				VK_FORMAT_R32G32B32_SFLOAT,
				0);
		// Location 1 : Normal
		vertices.attributeDescriptions[1] =
			vks::initializers::vertexInputAttributeDescription(
				VERTEX_BUFFER_BIND_ID,
				1,
				VK_FORMAT_R32G32B32_SFLOAT,
				sizeof(float) * 3);
		// Location 2 : Color
		vertices.attributeDescriptions[2] =
			vks::initializers::vertexInputAttributeDescription(
				VERTEX_BUFFER_BIND_ID,
				2,
				VK_FORMAT_R32G32B32_SFLOAT,
				sizeof(float) * 6);

		vertices.inputState = vks::initializers::pipelineVertexInputStateCreateInfo();
		vertices.inputState.vertexBindingDescriptionCount = static_cast<uint32_t>(vertices.bindingDescriptions.size());
		vertices.inputState.pVertexBindingDescriptions = vertices.bindingDescriptions.data();
		vertices.inputState.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertices.attributeDescriptions.size());
		vertices.inputState.pVertexAttributeDescriptions = vertices.attributeDescriptions.data();
	}

	void setupDescriptorPool()
	{
		// Example uses two ubos
		std::vector<VkDescriptorPoolSize> poolSizes = {
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3),
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2)
			//vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1),
		};

		VkDescriptorPoolCreateInfo descriptorPoolInfo =
			vks::initializers::descriptorPoolCreateInfo(
					static_cast<uint32_t>(poolSizes.size()),
					poolSizes.data(),
					4);

		VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &descriptorPool));
	}

	void setupDescriptorSetLayoutShading()
	{
		std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings =
		{
			// ubo model
			vks::initializers::descriptorSetLayoutBinding(
				VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				VK_SHADER_STAGE_GEOMETRY_BIT, //VK_SHADER_STAGE_VERTEX_BIT,
			0),
			// ubo lights
			vks::initializers::descriptorSetLayoutBinding(
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			VK_SHADER_STAGE_FRAGMENT_BIT,
			1),

			// ubo camera
			vks::initializers::descriptorSetLayoutBinding(
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			VK_SHADER_STAGE_GEOMETRY_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
			2),

			// cube map sampler
			vks::initializers::descriptorSetLayoutBinding(
				VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				VK_SHADER_STAGE_FRAGMENT_BIT,
				3)

		};

		VkDescriptorSetLayoutCreateInfo descriptorLayout =
			vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);

		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorLayout, nullptr, &descriptorSetLayout));

		VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo =
			vks::initializers::pipelineLayoutCreateInfo(&descriptorSetLayout, 1);

		/*
		 * Push Constants
		 */

		std::vector<VkPushConstantRange> pushConstantRanges = {
			vks::initializers::pushConstantRange(VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(Material::PushBlock), sizeof(glm::vec3)),
		};

		pPipelineLayoutCreateInfo.pushConstantRangeCount = 2;
		pPipelineLayoutCreateInfo.pPushConstantRanges = pushConstantRanges.data();

		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pPipelineLayoutCreateInfo, nullptr, &pipelineLayout));

		//setupDescriptorSetLayoutSky();
	}

	void setupDescriptorSetLayoutSky()
	{
		std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings =
		{
			// Binding 0 : Vertex shader uniform buffer
			vks::initializers::descriptorSetLayoutBinding(
				VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				VK_SHADER_STAGE_VERTEX_BIT,
				0),
			// Binding 1 : Fragment shader image sampler
			vks::initializers::descriptorSetLayoutBinding(
				VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				VK_SHADER_STAGE_FRAGMENT_BIT,
				1)
		};

		VkDescriptorSetLayoutCreateInfo descriptorLayout =
			vks::initializers::descriptorSetLayoutCreateInfo(
				setLayoutBindings.data(),
				setLayoutBindings.size());

		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorLayout, nullptr, &descriptorSetLayoutSky));

		VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo =
			vks::initializers::pipelineLayoutCreateInfo(
				&descriptorSetLayoutSky,
				1);

		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pPipelineLayoutCreateInfo, nullptr, &pipelineLayout));
	}

	void setupDescriptorSet()
	{

		// Image descriptor for the cube map texture
		VkDescriptorImageInfo textureDescriptor =
			vks::initializers::descriptorImageInfo(
				cubeMap.sampler,
				cubeMap.view,
				cubeMap.imageLayout);

		for (auto& gear : gears)
		{
			VkDescriptorSetAllocateInfo allocInfo =
				vks::initializers::descriptorSetAllocateInfo(
					descriptorPool,
					&descriptorSetLayout,
					1);

			VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &gear->descriptorSet));

			std::vector<VkWriteDescriptorSet> writeDescriptorSets = {

				// Binding 0 : Vertex shader uniform buffer
				vks::initializers::writeDescriptorSet(
					gear->descriptorSet,
					VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
					0,
					&gear->uniformBuffer.descriptor),
				vks::initializers::writeDescriptorSet(
							gear->descriptorSet,
							VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
							1,
							&uniformBuffers.lights.descriptor),
				vks::initializers::writeDescriptorSet(
							gear->descriptorSet,
							VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
							2,
							&uniformBuffers.camera.descriptor),


				// Binding 1 : Fragment shader cubemap sampler
				vks::initializers::writeDescriptorSet(
					gear->descriptorSet,
					VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
					3,
					&textureDescriptor)


			};

			vkUpdateDescriptorSets(device,
														 static_cast<uint32_t>(writeDescriptorSets.size()),
														 writeDescriptorSets.data(),
														 0,
														 nullptr);
		}

		//setupSkyBoxDesciptorSets();

	}

	void setupSkyBoxDesciptorSets() {

		// Sky box

		// Image descriptor for the cube map texture
		VkDescriptorImageInfo textureDescriptor =
			vks::initializers::descriptorImageInfo(
				cubeMap.sampler,
				cubeMap.view,
				cubeMap.imageLayout);

		VkDescriptorSetAllocateInfo allocInfo =
			vks::initializers::descriptorSetAllocateInfo(
				descriptorPool,
				&descriptorSetLayoutSky,
				1);

		// Sky box descriptor set
		VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &descriptorSets.skybox));

		printf("Checking descriptorSets.skybox for null: %p\n", descriptorSets.skybox);
		printf("Checking uniformBuffers.skybox.descriptor for null: %p\n", uniformBuffers.skybox.descriptor);
		printf("Checking cubeMap.sampler for null: %p\n", cubeMap.sampler);
		printf("Checking cubeMap.view for null: %p\n", cubeMap.view);
		printf("Checking cubeMap.imageLayout for null: %p\n", cubeMap.imageLayout);


		std::vector<VkWriteDescriptorSet> writeDescriptorSets =
		{

			// Binding 0 : Vertex shader uniform buffer
			vks::initializers::writeDescriptorSet(
				descriptorSets.skybox,
				VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				0,
				&uniformBuffers.skybox.descriptor),
						/*
			// Binding 1 : Fragment shader cubemap sampler
			vks::initializers::writeDescriptorSet(
				descriptorSets.skybox,
				VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				1,
				&textureDescriptor)
						*/
		};
		vkUpdateDescriptorSets(device, writeDescriptorSets.size(), writeDescriptorSets.data(), 0, NULL);


		printf("Checking uniformBuffers.skybox.descriptor for null: %p\n", uniformBuffers.skybox.descriptor);


	}

	void preparePipelines()
	{
		VkPipelineInputAssemblyStateCreateInfo inputAssemblyState =
			vks::initializers::pipelineInputAssemblyStateCreateInfo(
					VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
					0,
					VK_FALSE);


		VkPipelineRasterizationStateCreateInfo rasterizationState =
			vks::initializers::pipelineRasterizationStateCreateInfo(
					VK_POLYGON_MODE_FILL,
					VK_CULL_MODE_BACK_BIT,
					VK_FRONT_FACE_CLOCKWISE);


		VkPipelineRasterizationStateCreateInfo rasterizationStateSky =
			vks::initializers::pipelineRasterizationStateCreateInfo(
				VK_POLYGON_MODE_FILL,
				VK_CULL_MODE_BACK_BIT,
				VK_FRONT_FACE_COUNTER_CLOCKWISE,
				0);

		VkPipelineColorBlendAttachmentState blendAttachmentState =
			vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);

		VkPipelineColorBlendStateCreateInfo colorBlendState =
			vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);

		VkPipelineDepthStencilStateCreateInfo depthStencilState =
			vks::initializers::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);

		// We use two viewports
		VkPipelineViewportStateCreateInfo viewportState =
			vks::initializers::pipelineViewportStateCreateInfo(2, 2, 0);

		VkPipelineMultisampleStateCreateInfo multisampleState =
			vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT);

		std::vector<VkDynamicState> dynamicStateEnables = {
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR,
			VK_DYNAMIC_STATE_LINE_WIDTH
		};
		VkPipelineDynamicStateCreateInfo dynamicState =
			vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables);


		// Tessellation pipeline
		// Load shaders
		std::array<VkPipelineShaderStageCreateInfo, 3> shaderStages;

		VkGraphicsPipelineCreateInfo pipelineCreateInfo =
			vks::initializers::pipelineCreateInfo(pipelineLayout, renderPass);

		// Vertex bindings an attributes
		std::vector<VkVertexInputBindingDescription> vertexInputBindings = {
			vks::initializers::vertexInputBindingDescription(0, vertexLayout.stride(), VK_VERTEX_INPUT_RATE_VERTEX),
		};
		std::vector<VkVertexInputAttributeDescription> vertexInputAttributes = {
			vks::initializers::vertexInputAttributeDescription(0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0),					// Location 0: Position			
			vks::initializers::vertexInputAttributeDescription(0, 1, VK_FORMAT_R32G32B32_SFLOAT, sizeof(float) * 3),	// Location 1: Normals			
			vks::initializers::vertexInputAttributeDescription(0, 2, VK_FORMAT_R32G32B32_SFLOAT, sizeof(float) * 6),	// Location 2: Color

		};
		VkPipelineVertexInputStateCreateInfo vertexInputState = vks::initializers::pipelineVertexInputStateCreateInfo();
		vertexInputState.vertexBindingDescriptionCount = static_cast<uint32_t>(vertexInputBindings.size());
		vertexInputState.pVertexBindingDescriptions = vertexInputBindings.data();
		vertexInputState.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexInputAttributes.size());
		vertexInputState.pVertexAttributeDescriptions = vertexInputAttributes.data();

		pipelineCreateInfo.pVertexInputState = &vertexInputState;
		pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
		pipelineCreateInfo.pRasterizationState = &rasterizationState;
		pipelineCreateInfo.pColorBlendState = &colorBlendState;
		pipelineCreateInfo.pMultisampleState = &multisampleState;
		pipelineCreateInfo.pViewportState = &viewportState;
		pipelineCreateInfo.pDepthStencilState = &depthStencilState;
		pipelineCreateInfo.pDynamicState = &dynamicState;
		pipelineCreateInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
		pipelineCreateInfo.pStages = shaderStages.data();
		pipelineCreateInfo.renderPass = renderPass;



		shaderStages[0] = loadShader(getAssetPath() + "shaders/xrgears/scene.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getAssetPath() + "shaders/xrgears/scene.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		// A geometry shader is used to output geometry to multiple viewports in one single pass
		// See the "invoctations" decorator of the layout input in the shader
		shaderStages[2] = loadShader(getAssetPath() + "shaders/xrgears/multiview.geom.spv", VK_SHADER_STAGE_GEOMETRY_BIT);
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipeline));




		/*
		// Skybox pipeline (background cube)
		std::array<VkPipelineShaderStageCreateInfo,2> shaderStages2;

		shaderStages2[0] = loadShader(getAssetPath() + "shaders/cubemap/skybox.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages2[1] = loadShader(getAssetPath() + "shaders/cubemap/skybox.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		*/

		std::array<VkPipelineShaderStageCreateInfo,3> shaderStagesSky;

		shaderStagesSky[0] = loadShader(getAssetPath() + "shaders/debug/debug.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStagesSky[1] = loadShader(getAssetPath() + "shaders/debug/debug.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		shaderStagesSky[2] = loadShader(getAssetPath() + "shaders/debug/debug.geom.spv", VK_SHADER_STAGE_GEOMETRY_BIT);
		//VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipeline));

		pipelineCreateInfo.stageCount = shaderStagesSky.size();
		pipelineCreateInfo.pStages = shaderStagesSky.data();
		pipelineCreateInfo.pRasterizationState = &rasterizationStateSky;

		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipelines.skybox));



/*
		VkGraphicsPipelineCreateInfo pipelineCreateInfoSky =
			vks::initializers::pipelineCreateInfo(
				pipelineLayout,
				renderPass,
				0);

		pipelineCreateInfoSky.pVertexInputState = &vertices.inputState;
		pipelineCreateInfoSky.pInputAssemblyState = &inputAssemblyState;
		pipelineCreateInfoSky.pRasterizationState = &rasterizationState;
		pipelineCreateInfoSky.pColorBlendState = &colorBlendState;
		pipelineCreateInfoSky.pMultisampleState = &multisampleState;
		pipelineCreateInfoSky.pViewportState = &viewportState;
		pipelineCreateInfoSky.pDepthStencilState = &depthStencilState;
		pipelineCreateInfoSky.pDynamicState = &dynamicState;
		pipelineCreateInfoSky.stageCount = shaderStages2.size();
		pipelineCreateInfoSky.pStages = shaderStages2.data();
*/


		//pipelineCreateInfo.stageCount = shaderStages2.size();
		//pipelineCreateInfo.pStages = shaderStages2.data();

		//VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipelines.skybox));

	}


	void createUniformBuffer(vks::Buffer *buffer,  VkDeviceSize size) {
		VK_CHECK_RESULT(vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			buffer,
			size));

		// Map persistent
		VK_CHECK_RESULT(buffer->map());
	}

	// Prepare and initialize uniform buffer containing shader uniforms
	void prepareUniformBuffers()
	{
		createUniformBuffer(&uniformBuffers.lights, sizeof(uboLights));
		createUniformBuffer(&uniformBuffers.camera, sizeof(uboCamera));

		for (auto& gear : gears)
			gear->prepareUniformBuffer();



		// Skybox vertex shader uniform buffer
		VK_CHECK_RESULT(vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&uniformBuffers.skybox,
			sizeof(uboVS)));

		// Map persistent
		VK_CHECK_RESULT(uniformBuffers.skybox.map());



		updateUniformBuffers();


	}

	void updateUniformBuffers()
	{
		updateCamera();

		StereoView sv = {};
		sv.view[0] = uboCamera.view[0];
		sv.view[1] = uboCamera.view[1];

		for (auto& gear : gears)
			gear->updateUniformBuffer(sv, timer);

		updateLights();

		uboVS.projection = uboCamera.projection[0];
		uboVS.model = glm::mat4();

		memcpy(uniformBuffers.skybox.mapped, &uboVS, sizeof(uboVS));

	}

	void updateCamera() {
		// Geometry shader matrices for the two viewports
		// See http://paulbourke.net/stereographics/stereorender/

		// Calculate some variables
		float aspectRatio = (float)(width * 0.5f) / (float)height;
		float wd2 = zNear * tan(glm::radians(fov / 2.0f));
		float ndfl = zNear / focalLength;
		float left, right;
		float top = wd2;
		float bottom = -wd2;

		glm::vec3 camFront;
		camFront.x = -cos(glm::radians(rotation.x)) * sin(glm::radians(rotation.y));
		camFront.y = sin(glm::radians(rotation.x));
		camFront.z = cos(glm::radians(rotation.x)) * cos(glm::radians(rotation.y));
		camFront = glm::normalize(camFront);
		glm::vec3 camRight = glm::normalize(glm::cross(camFront, glm::vec3(0.0f, 1.0f, 0.0f)));

		glm::mat4 rotM = glm::mat4();
		glm::mat4 transM;

		rotM = glm::rotate(rotM, glm::radians(camera.rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
		rotM = glm::rotate(rotM, glm::radians(camera.rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
		rotM = glm::rotate(rotM, glm::radians(camera.rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

		// Left eye
		left = -aspectRatio * wd2 + 0.5f * eyeSeparation * ndfl;
		right = aspectRatio * wd2 + 0.5f * eyeSeparation * ndfl;

		transM = glm::translate(glm::mat4(), camera.position - camRight * (eyeSeparation / 2.0f));

		uboCamera.projection[0] = glm::frustum(left, right, bottom, top, zNear, zFar);
		uboCamera.view[0] = rotM * transM;

		// Right eye
		left = -aspectRatio * wd2 - 0.5f * eyeSeparation * ndfl;
		right = aspectRatio * wd2 - 0.5f * eyeSeparation * ndfl;

		transM = glm::translate(glm::mat4(), camera.position + camRight * (eyeSeparation / 2.0f));

		uboCamera.projection[1] = glm::frustum(left, right, bottom, top, zNear, zFar);
		uboCamera.view[1] = rotM * transM;

		uboCamera.position = camera.position * -1.0f;

		memcpy(uniformBuffers.camera.mapped, &uboCamera, sizeof(uboCamera));
	}

	void updateLights()
	{
		const float p = 15.0f;
		uboLights.lights[0] = glm::vec4(-p, -p*0.5f, -p, 1.0f);
		uboLights.lights[1] = glm::vec4(-p, -p*0.5f,  p, 1.0f);
		uboLights.lights[2] = glm::vec4( p, -p*0.5f,  p, 1.0f);
		uboLights.lights[3] = glm::vec4( p, -p*0.5f, -p, 1.0f);

		if (!paused)
		{
			uboLights.lights[0].x = sin(glm::radians(timer * 360.0f)) * 20.0f;
			uboLights.lights[0].z = cos(glm::radians(timer * 360.0f)) * 20.0f;
			uboLights.lights[1].x = cos(glm::radians(timer * 360.0f)) * 20.0f;
			uboLights.lights[1].y = sin(glm::radians(timer * 360.0f)) * 20.0f;
		}

		memcpy(uniformBuffers.lights.mapped, &uboLights, sizeof(uboLights));
	}

	void loadCubemap(std::string filename, VkFormat format, bool forceLinearTiling)
	{
#if defined(__ANDROID__)
		// Textures are stored inside the apk on Android (compressed)
		// So they need to be loaded via the asset manager
		AAsset* asset = AAssetManager_open(androidApp->activity->assetManager, filename.c_str(), AASSET_MODE_STREAMING);
		assert(asset);
		size_t size = AAsset_getLength(asset);
		assert(size > 0);

		void *textureData = malloc(size);
		AAsset_read(asset, textureData, size);
		AAsset_close(asset);

		gli::texture_cube texCube(gli::load((const char*)textureData, size));
#else
		gli::texture_cube texCube(gli::load(filename));
#endif

		assert(!texCube.empty());

		cubeMap.width = texCube.extent().x;
		cubeMap.height = texCube.extent().y;
		cubeMap.mipLevels = texCube.levels();

		VkMemoryAllocateInfo memAllocInfo = vks::initializers::memoryAllocateInfo();
		VkMemoryRequirements memReqs;

		// Create a host-visible staging buffer that contains the raw image data
		VkBuffer stagingBuffer;
		VkDeviceMemory stagingMemory;

		VkBufferCreateInfo bufferCreateInfo = vks::initializers::bufferCreateInfo();
		bufferCreateInfo.size = texCube.size();
		// This buffer is used as a transfer source for the buffer copy
		bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		VK_CHECK_RESULT(vkCreateBuffer(device, &bufferCreateInfo, nullptr, &stagingBuffer));

		// Get memory requirements for the staging buffer (alignment, memory type bits)
		vkGetBufferMemoryRequirements(device, stagingBuffer, &memReqs);
		memAllocInfo.allocationSize = memReqs.size;
		// Get memory type index for a host visible buffer
		memAllocInfo.memoryTypeIndex = vulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		VK_CHECK_RESULT(vkAllocateMemory(device, &memAllocInfo, nullptr, &stagingMemory));
		VK_CHECK_RESULT(vkBindBufferMemory(device, stagingBuffer, stagingMemory, 0));

		// Copy texture data into staging buffer
		uint8_t *data;
		VK_CHECK_RESULT(vkMapMemory(device, stagingMemory, 0, memReqs.size, 0, (void **)&data));
		memcpy(data, texCube.data(), texCube.size());
		vkUnmapMemory(device, stagingMemory);

		// Create optimal tiled target image
		VkImageCreateInfo imageCreateInfo = vks::initializers::imageCreateInfo();
		imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
		imageCreateInfo.format = format;
		imageCreateInfo.mipLevels = cubeMap.mipLevels;
		imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
		imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageCreateInfo.extent = { cubeMap.width, cubeMap.height, 1 };
		imageCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		// Cube faces count as array layers in Vulkan
		imageCreateInfo.arrayLayers = 6;
		// This flag is required for cube map images
		imageCreateInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

		VK_CHECK_RESULT(vkCreateImage(device, &imageCreateInfo, nullptr, &cubeMap.image));

		vkGetImageMemoryRequirements(device, cubeMap.image, &memReqs);

		memAllocInfo.allocationSize = memReqs.size;
		memAllocInfo.memoryTypeIndex = vulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		VK_CHECK_RESULT(vkAllocateMemory(device, &memAllocInfo, nullptr, &cubeMap.deviceMemory));
		VK_CHECK_RESULT(vkBindImageMemory(device, cubeMap.image, cubeMap.deviceMemory, 0));

		VkCommandBuffer copyCmd = VulkanExampleBase::createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

		// Setup buffer copy regions for each face including all of it's miplevels
		std::vector<VkBufferImageCopy> bufferCopyRegions;
		uint32_t offset = 0;

		for (uint32_t face = 0; face < 6; face++)
		{
			for (uint32_t level = 0; level < cubeMap.mipLevels; level++)
			{
				VkBufferImageCopy bufferCopyRegion = {};
				bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				bufferCopyRegion.imageSubresource.mipLevel = level;
				bufferCopyRegion.imageSubresource.baseArrayLayer = face;
				bufferCopyRegion.imageSubresource.layerCount = 1;
				bufferCopyRegion.imageExtent.width = texCube[face][level].extent().x;
				bufferCopyRegion.imageExtent.height = texCube[face][level].extent().y;
				bufferCopyRegion.imageExtent.depth = 1;
				bufferCopyRegion.bufferOffset = offset;

				bufferCopyRegions.push_back(bufferCopyRegion);

				// Increase offset into staging buffer for next level / face
				offset += texCube[face][level].size();
			}
		}

		// Image barrier for optimal image (target)
		// Set initial layout for all array layers (faces) of the optimal (target) tiled texture
		VkImageSubresourceRange subresourceRange = {};
		subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		subresourceRange.baseMipLevel = 0;
		subresourceRange.levelCount = cubeMap.mipLevels;
		subresourceRange.layerCount = 6;

		vks::tools::setImageLayout(
			copyCmd,
			cubeMap.image,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			subresourceRange);

		// Copy the cube map faces from the staging buffer to the optimal tiled image
		vkCmdCopyBufferToImage(
			copyCmd,
			stagingBuffer,
			cubeMap.image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			static_cast<uint32_t>(bufferCopyRegions.size()),
			bufferCopyRegions.data()
			);

		// Change texture image layout to shader read after all faces have been copied
		cubeMap.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		vks::tools::setImageLayout(
			copyCmd,
			cubeMap.image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			cubeMap.imageLayout,
			subresourceRange);

		VulkanExampleBase::flushCommandBuffer(copyCmd, queue, true);

		// Create sampler
		VkSamplerCreateInfo sampler = vks::initializers::samplerCreateInfo();
		sampler.magFilter = VK_FILTER_LINEAR;
		sampler.minFilter = VK_FILTER_LINEAR;
		sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		sampler.addressModeV = sampler.addressModeU;
		sampler.addressModeW = sampler.addressModeU;
		sampler.mipLodBias = 0.0f;
		sampler.compareOp = VK_COMPARE_OP_NEVER;
		sampler.minLod = 0.0f;
		sampler.maxLod = cubeMap.mipLevels;
		sampler.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		sampler.maxAnisotropy = 1.0f;
		if (vulkanDevice->features.samplerAnisotropy)
		{
			sampler.maxAnisotropy = vulkanDevice->properties.limits.maxSamplerAnisotropy;
			sampler.anisotropyEnable = VK_TRUE;
		}
		VK_CHECK_RESULT(vkCreateSampler(device, &sampler, nullptr, &cubeMap.sampler));

		// Create image view
		VkImageViewCreateInfo view = vks::initializers::imageViewCreateInfo();
		// Cube map view type
		view.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
		view.format = format;
		view.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
		view.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
		// 6 array layers (faces)
		view.subresourceRange.layerCount = 6;
		// Set number of mip levels
		view.subresourceRange.levelCount = cubeMap.mipLevels;
		view.image = cubeMap.image;
		VK_CHECK_RESULT(vkCreateImageView(device, &view, nullptr, &cubeMap.view));

		// Clean up staging resources
		vkFreeMemory(device, stagingMemory, nullptr);
		vkDestroyBuffer(device, stagingBuffer, nullptr);
	}

	void loadTextures()
	{
		// Vulkan core supports three different compressed texture formats
		// As the support differs between implemementations we need to check device features and select a proper format and file
		std::string filename;
		VkFormat format;
		if (deviceFeatures.textureCompressionBC) {
			filename = "cubemap_yokohama_bc3_unorm.ktx";
			format = VK_FORMAT_BC2_UNORM_BLOCK;
		}
		else if (deviceFeatures.textureCompressionASTC_LDR) {
			filename = "cubemap_yokohama_astc_8x8_unorm.ktx";
			format = VK_FORMAT_ASTC_8x8_UNORM_BLOCK;
		}
		else if (deviceFeatures.textureCompressionETC2) {
			filename = "cubemap_yokohama_etc2_unorm.ktx";
			format = VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK;
		}
		else {
			vks::tools::exitFatal("Device does not support any compressed texture format!", "Error");
		}

		printf("Using texture %s\n", filename.c_str());

		loadCubemap(getAssetPath() + "textures/" + filename, format, false);
	}

	void draw()
	{
		VulkanExampleBase::prepareFrame();
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &drawCmdBuffers[currentBuffer];
		VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));
		VulkanExampleBase::submitFrame();
	}

	void prepare()
	{
		VulkanExampleBase::prepare();
		loadTextures();
		loadAssets();
		prepareVertices();
		prepareUniformBuffers();
		setupDescriptorSetLayoutShading();
		preparePipelines();
		setupDescriptorPool();
		setupDescriptorSet();
		buildCommandBuffers();
		prepared = true;
	}

	virtual void render()
	{
		if (!prepared)
			return;

		vkDeviceWaitIdle(device);
		draw();
		vkDeviceWaitIdle(device);
		if (!paused)
		{
			updateUniformBuffers();
		}
	}

	virtual void viewChanged()
	{
		updateUniformBuffers();
	}

	void changeEyeSeparation(float delta)
	{
		eyeSeparation += delta;
		updateUniformBuffers();
	}

	virtual void keyPressed(uint32_t keyCode)
	{
		switch (keyCode)
		{
		case KEY_KPADD:
		case GAMEPAD_BUTTON_R1:
			changeEyeSeparation(0.005);
			break;
		case KEY_KPSUB:
		case GAMEPAD_BUTTON_L1:
			changeEyeSeparation(-0.005);
			break;
		}
	}

};

VULKAN_EXAMPLE_MAIN()
