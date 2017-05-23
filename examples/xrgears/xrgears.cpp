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

#include <vulkan/vulkan.h>
#include "vulkangear.h"
#include "vulkanexamplebase.h"
#include "VulkanModel.hpp"

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

	vks::Model scene;

	struct {
		VkPipelineVertexInputStateCreateInfo inputState;
		std::vector<VkVertexInputBindingDescription> bindingDescriptions;
		std::vector<VkVertexInputAttributeDescription> attributeDescriptions;
	} vertices;

	std::vector<VulkanGear*> gears;

	/*
	struct UBOGS {
		glm::mat4 projection[2];
		glm::mat4 view[2];
		glm::mat4 model;
		glm::mat4 normal;
		glm::vec4 lightPos = glm::vec4(-2.5f, -3.5f, 0.0f, 1.0f);
	} uboGS;
	*/

	//vks::Buffer uniformBufferGS;

	VkPipeline pipeline;
	VkPipelineLayout pipelineLayout;
	//VkDescriptorSet descriptorSet;
	VkDescriptorSetLayout descriptorSetLayout;

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
	}

	~VulkanExample()
	{
		vkDestroyPipeline(device, pipeline, nullptr);

		vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
		vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);

		scene.destroy();

		//uniformBufferGS.destroy();

		for (auto& gear : gears)
		{
			delete(gear);
		}
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
			{
				vks::debugmarker::beginRegion(drawCmdBuffers[i], "Render stuff?", glm::vec4(0.3f, 0.94f, 1.0f, 1.0f));
			}


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

//			vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
/*
			VkDeviceSize offsets[1] = { 0 };
			vkCmdBindVertexBuffers(drawCmdBuffers[i], 0, 1, &scene.vertices.buffer, offsets);
			vkCmdBindIndexBuffer(drawCmdBuffers[i], scene.indices.buffer, 0, VK_INDEX_TYPE_UINT32);
			vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
			vkCmdDrawIndexed(drawCmdBuffers[i], scene.indexCount, 1, 0, 0, 0);
*/

			vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

			for (auto& gear : gears)
			{
				gear->draw(drawCmdBuffers[i], pipelineLayout);
			}



			vkCmdEndRenderPass(drawCmdBuffers[i]);

			if (vks::debugmarker::active)
			{
				vks::debugmarker::endRegion(drawCmdBuffers[i]);
			}

			VK_CHECK_RESULT(vkEndCommandBuffer(drawCmdBuffers[i]));
		}
	}

	void loadAssets()
	{
		scene.loadFromFile(getAssetPath() + "models/sampleroom.dae", vertexLayout, 0.25f, vulkanDevice, queue);		
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
			//vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1),
		};

		VkDescriptorPoolCreateInfo descriptorPoolInfo =
			vks::initializers::descriptorPoolCreateInfo(
					static_cast<uint32_t>(poolSizes.size()),
					poolSizes.data(),
					3);

		VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &descriptorPool));
	}

	void setupDescriptorSetLayout()
	{
		std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings =
		{

			// Binding 0 : Vertex shader uniform buffer
			vks::initializers::descriptorSetLayoutBinding(
				VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				VK_SHADER_STAGE_GEOMETRY_BIT, //VK_SHADER_STAGE_VERTEX_BIT,
			0),
/*
			// Binding 1: Geometry shader ubo
			vks::initializers::descriptorSetLayoutBinding(
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			VK_SHADER_STAGE_GEOMETRY_BIT,
			1)
				*/
		};

		VkDescriptorSetLayoutCreateInfo descriptorLayout =
			vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);

		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorLayout, nullptr, &descriptorSetLayout));

		VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo =
			vks::initializers::pipelineLayoutCreateInfo(&descriptorSetLayout, 1);

		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pPipelineLayoutCreateInfo, nullptr, &pipelineLayout));
	}

	void setupDescriptorSet()
	{

/*
		for (auto& gear : gears)
		{
			gear->setupDescriptorSet(descriptorPool, descriptorSetLayout);
		}
*/

#if 0
		std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
/*
			// Binding 0 : Vertex shader uniform buffer
			vks::initializers::writeDescriptorSet(
				descriptorSet,
				VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				0,
				&gear->uniformBuffer.descriptor),
*/
			// Binding 1 :Geometry shader ubo
			vks::initializers::writeDescriptorSet(descriptorSet,
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
			&uniformBufferGS.descriptor),
		};

		for (auto& gear : gears)
		{
			gear->setupDescriptorSet(descriptorPool, descriptorSetLayout, &writeDescriptorSets);
		}
#endif




		for (auto& gear : gears)
		{
			VkDescriptorSetAllocateInfo allocInfo =
				vks::initializers::descriptorSetAllocateInfo(
					descriptorPool,
					&descriptorSetLayout,
					1);

			VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &gear->descriptorSet));

			/*
			// Binding 0 : Vertex shader uniform buffer
			VkWriteDescriptorSet writeDescriptorSet =
				vks::initializers::writeDescriptorSet(
					descriptorSet,
					VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
					0,
					&gear->uniformBuffer.descriptor);

			vkUpdateDescriptorSets(vulkanDevice->logicalDevice, 1,
														 &writeDescriptorSet, 0, NULL);
		}
		*/

			std::vector<VkWriteDescriptorSet> writeDescriptorSets = {

				// Binding 0 : Vertex shader uniform buffer
				vks::initializers::writeDescriptorSet(
					gear->descriptorSet,
					VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
					0,
					&gear->uniformBuffer.descriptor),

				/*
				// Binding 1 :Geometry shader ubo
				vks::initializers::writeDescriptorSet(gear->descriptorSet,
				VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
				&uniformBufferGS.descriptor),
					*/
			};

			vkUpdateDescriptorSets(device,
														 static_cast<uint32_t>(writeDescriptorSets.size()),
														 writeDescriptorSets.data(),
														 0,
														 nullptr);

		}

	/*
		// setup multiview uniform buffer
		VkDescriptorSetAllocateInfo allocInfo2 =
			vks::initializers::descriptorSetAllocateInfo(
				descriptorPool,
				&descriptorSetLayout,
				1);

		VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo2, &descriptorSet));
*/
/*
		std::vector<VkWriteDescriptorSet> writeDescriptorSets = {

			// Binding 0 : Vertex shader uniform buffer
			vks::initializers::writeDescriptorSet(
				descriptorSet,
				VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				0,
				&uniformBuffer.descriptor),

			// Binding 1 :Geometry shader ubo
			vks::initializers::writeDescriptorSet(descriptorSet,
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
			&uniformBufferGS.descriptor),
		};
			*/
/*
		vkUpdateDescriptorSets(device,
													 static_cast<uint32_t>(writeDescriptorSets.size()),
													 writeDescriptorSets.data(),
													 0,
													 nullptr);
*/


	}

	void preparePipelines()
	{
		VkPipelineInputAssemblyStateCreateInfo inputAssemblyState =
			vks::initializers::pipelineInputAssemblyStateCreateInfo(
					VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);

		VkPipelineRasterizationStateCreateInfo rasterizationState =
			vks::initializers::pipelineRasterizationStateCreateInfo(
					VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_CLOCKWISE);

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
	}

	// Prepare and initialize uniform buffer containing shader uniforms
	void prepareUniformBuffers()
	{
		/*
		// Geometry shader uniform buffer block
		VK_CHECK_RESULT(vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&uniformBufferGS,
			sizeof(uboGS)));

		// Map persistent
		VK_CHECK_RESULT(uniformBufferGS.map());
*/

/*
		for (auto& gear : gears)
		{
			gear->prepareUniformBuffer();
		}
*/
		updateUniformBuffers();

	}

	void updateUniformBuffers()
	{
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


		StereoViewProjection svp = {};

		svp.projection[0] = glm::frustum(left, right, bottom, top, zNear, zFar);
		svp.view[0] = rotM * transM;

		// Right eye
		left = -aspectRatio * wd2 - 0.5f * eyeSeparation * ndfl;
		right = aspectRatio * wd2 - 0.5f * eyeSeparation * ndfl;

		transM = glm::translate(glm::mat4(), camera.position + camRight * (eyeSeparation / 2.0f));

		svp.projection[1] = glm::frustum(left, right, bottom, top, zNear, zFar);
		svp.view[1] = rotM * transM;

		//uboGS.model = glm::mat4();
		//uboGS.normal = glm::mat4();

		//memcpy(uniformBufferGS.mapped, &uboGS, sizeof(uboGS));

/*
		for (auto& gear : gears)
		{
			glm::mat4 model = gear->getModelMatrix(glm::vec3(), timer * 360.0f);
		}
		*/

		glm::mat4 perspective = glm::perspective(glm::radians(60.0f), (float)width / (float)height, 0.001f, 256.0f);
		for (auto& gear : gears)
		{
			gear->updateUniformBuffer(svp, rotation, zoom, timer * 360.0f);
		}

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
		loadAssets();
		prepareVertices();
		prepareUniformBuffers();
		setupDescriptorSetLayout();
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
		/*
		draw();
		*/

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
