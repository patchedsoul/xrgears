/*
* Vulkan Example - Cube map texture loading and displaying
*
* Copyright (C) 2016 by Sascha Willems - www.saschawillems.de
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
#include "system/vikApplication.hpp"
#include "render/vikModel.hpp"
#include "render/vikTexture.hpp"
#include "scene/vikCameraArcBall.hpp"

#define VERTEX_BUFFER_BIND_ID 0
#define ENABLE_VALIDATION false

class XRCubeMap : public vik::Application
{
public:
	bool displaySkybox = true;

  vik::TextureCubeMap cubeMap;

	struct {
		VkPipelineVertexInputStateCreateInfo inputState;
		std::vector<VkVertexInputBindingDescription> bindingDescriptions;
		std::vector<VkVertexInputAttributeDescription> attributeDescriptions;
	} vertices;

	// Vertex layout for the models
  vik::VertexLayout vertexLayout = vik::VertexLayout({
    vik::VERTEX_COMPONENT_POSITION,
    vik::VERTEX_COMPONENT_NORMAL,
    vik::VERTEX_COMPONENT_UV,
	});

	struct Meshes {
    vik::Model skybox;
    std::vector<vik::Model> objects;
		uint32_t objectIndex = 0;
	} models;

	struct {
    vik::Buffer object;
    vik::Buffer skybox;
	} uniformBuffers;

	struct UBOVS {
		glm::mat4 projection;
		glm::mat4 model;
		float lodBias = 0.0f;
	} uboVS;

	struct {
		VkPipeline skybox;
		VkPipeline reflect;
	} pipelines;

	struct {
		VkDescriptorSet object;
		VkDescriptorSet skybox;
	} descriptorSets;

	VkPipelineLayout pipelineLayout;
	VkDescriptorSetLayout descriptorSetLayout;

  XRCubeMap(int argc, char *argv[]) : vik::Application(argc, argv) {
    settings.enable_text_overlay = true;
    name = "Cube map viewer";
    camera = new vik::CameraArcBall();
    camera->zoom = -4.0f;
    camera->rotationSpeed = 0.25f;
    camera->rotation = { -7.25f, -120.0f, 0.0f };
    camera->set_view_updated_cb([this]() { viewUpdated = true; });
	}

  ~XRCubeMap()
	{
		// Clean up used Vulkan resources 
		// Note : Inherited destructor cleans up resources stored in base class

		// Clean up texture resources
    vkDestroyImageView(renderer->device, cubeMap.view, nullptr);
    vkDestroyImage(renderer->device, cubeMap.image, nullptr);
    vkDestroySampler(renderer->device, cubeMap.sampler, nullptr);
    vkFreeMemory(renderer->device, cubeMap.deviceMemory, nullptr);

    vkDestroyPipeline(renderer->device, pipelines.skybox, nullptr);
    vkDestroyPipeline(renderer->device, pipelines.reflect, nullptr);

    vkDestroyPipelineLayout(renderer->device, pipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(renderer->device, descriptorSetLayout, nullptr);

		for (auto& model : models.objects) {
			model.destroy();
		}
		models.skybox.destroy();

		uniformBuffers.object.destroy();
		uniformBuffers.skybox.destroy();
	}

	void loadTextures()
	{
		// Vulkan core supports three different compressed texture formats
		// As the support differs between implemementations we need to check device features and select a proper format and file
		std::string filename;
		VkFormat format;
    if (renderer->deviceFeatures.textureCompressionBC) {
			filename = "cubemap_yokohama_bc3_unorm.ktx";
			format = VK_FORMAT_BC2_UNORM_BLOCK;
		}
    else if (renderer->deviceFeatures.textureCompressionASTC_LDR) {
			filename = "cubemap_yokohama_astc_8x8_unorm.ktx";
			format = VK_FORMAT_ASTC_8x8_UNORM_BLOCK;
		}
    else if (renderer->deviceFeatures.textureCompressionETC2) {
			filename = "cubemap_yokohama_etc2_unorm.ktx";
			format = VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK;
		}
		else {
      vik_log_f("Device does not support any compressed texture format!");
		}

    cubeMap.loadFromFile(vik::Assets::getTexturePath() + filename, format, renderer->vksDevice, renderer->queue);
	}

	void reBuildCommandBuffers()
	{
    if (!renderer->check_command_buffers())
		{
      renderer->destroy_command_buffers();
      renderer->allocate_command_buffers(window->get_swap_chain()->image_count);
		}
		buildCommandBuffers();
	}

	void buildCommandBuffers()
	{
    VkCommandBufferBeginInfo cmdBufInfo = vik::initializers::commandBufferBeginInfo();

		VkClearValue clearValues[2];
    clearValues[0].color = renderer->defaultClearColor;
		clearValues[1].depthStencil = { 1.0f, 0 };

    VkRenderPassBeginInfo renderPassBeginInfo = vik::initializers::renderPassBeginInfo();
    renderPassBeginInfo.renderPass = renderer->render_pass;
		renderPassBeginInfo.renderArea.offset.x = 0;
		renderPassBeginInfo.renderArea.offset.y = 0;
    renderPassBeginInfo.renderArea.extent.width = renderer->width;
    renderPassBeginInfo.renderArea.extent.height = renderer->height;
		renderPassBeginInfo.clearValueCount = 2;
		renderPassBeginInfo.pClearValues = clearValues;

    for (int32_t i = 0; i < renderer->cmd_buffers.size(); ++i)
		{
			// Set target frame buffer
      renderPassBeginInfo.framebuffer = renderer->frame_buffers[i];

      vik_log_check(vkBeginCommandBuffer(renderer->cmd_buffers[i], &cmdBufInfo));

      vkCmdBeginRenderPass(renderer->cmd_buffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

      VkViewport viewport = vik::initializers::viewport((float)renderer->width,	(float)renderer->height, 0.0f, 1.0f);
      vkCmdSetViewport(renderer->cmd_buffers[i], 0, 1, &viewport);

      VkRect2D scissor = vik::initializers::rect2D(renderer->width,	renderer->height,	0, 0);
      vkCmdSetScissor(renderer->cmd_buffers[i], 0, 1, &scissor);

			VkDeviceSize offsets[1] = { 0 };

			// Skybox
			if (displaySkybox)
			{
        vkCmdBindDescriptorSets(renderer->cmd_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets.skybox, 0, NULL);
        vkCmdBindVertexBuffers(renderer->cmd_buffers[i], VERTEX_BUFFER_BIND_ID, 1, &models.skybox.vertices.buffer, offsets);
        vkCmdBindIndexBuffer(renderer->cmd_buffers[i], models.skybox.indices.buffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdBindPipeline(renderer->cmd_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.skybox);
        vkCmdDrawIndexed(renderer->cmd_buffers[i], models.skybox.indexCount, 1, 0, 0, 0);
			}

			// 3D object
      vkCmdBindDescriptorSets(renderer->cmd_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets.object, 0, NULL);
      vkCmdBindVertexBuffers(renderer->cmd_buffers[i], VERTEX_BUFFER_BIND_ID, 1, &models.objects[models.objectIndex].vertices.buffer, offsets);
      vkCmdBindIndexBuffer(renderer->cmd_buffers[i], models.objects[models.objectIndex].indices.buffer, 0, VK_INDEX_TYPE_UINT32);
      vkCmdBindPipeline(renderer->cmd_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.reflect);
      vkCmdDrawIndexed(renderer->cmd_buffers[i], models.objects[models.objectIndex].indexCount, 1, 0, 0, 0);

      vkCmdEndRenderPass(renderer->cmd_buffers[i]);

      vik_log_check(vkEndCommandBuffer(renderer->cmd_buffers[i]));
		}
	}

	void loadMeshes()
	{
		// Skybox
    models.skybox.loadFromFile(vik::Assets::getAssetPath() + "models/cube.obj", vertexLayout, 0.05f, renderer->vksDevice, renderer->queue);
		// Objects
		std::vector<std::string> filenames = { "sphere.obj", "teapot.dae", "torusknot.obj" };
		for (auto file : filenames) {
      vik::Model model;
      model.loadFromFile(vik::Assets::getAssetPath() + "models/" + file, vertexLayout, 0.05f, renderer->vksDevice, renderer->queue);
			models.objects.push_back(model);
		}
	}

	void setupVertexDescriptions()
	{
		// Binding description
		vertices.bindingDescriptions.resize(1);
		vertices.bindingDescriptions[0] =
      vik::initializers::vertexInputBindingDescription(
				VERTEX_BUFFER_BIND_ID,
				vertexLayout.stride(),
				VK_VERTEX_INPUT_RATE_VERTEX);

		// Attribute descriptions
		// Describes memory layout and shader positions
		vertices.attributeDescriptions.resize(3);
		// Location 0 : Position
		vertices.attributeDescriptions[0] =
      vik::initializers::vertexInputAttributeDescription(
				VERTEX_BUFFER_BIND_ID,
				0,
				VK_FORMAT_R32G32B32_SFLOAT,
				0);
		// Location 1 : Normal
		vertices.attributeDescriptions[1] =
      vik::initializers::vertexInputAttributeDescription(
				VERTEX_BUFFER_BIND_ID,
				1,
				VK_FORMAT_R32G32B32_SFLOAT,
				sizeof(float) * 3);
		// Location 2 : Texture coordinates
		vertices.attributeDescriptions[2] =
      vik::initializers::vertexInputAttributeDescription(
				VERTEX_BUFFER_BIND_ID,
				2,
				VK_FORMAT_R32G32_SFLOAT,
				sizeof(float) * 5);

    vertices.inputState = vik::initializers::pipelineVertexInputStateCreateInfo();
		vertices.inputState.vertexBindingDescriptionCount = vertices.bindingDescriptions.size();
		vertices.inputState.pVertexBindingDescriptions = vertices.bindingDescriptions.data();
		vertices.inputState.vertexAttributeDescriptionCount = vertices.attributeDescriptions.size();
		vertices.inputState.pVertexAttributeDescriptions = vertices.attributeDescriptions.data();
	}

	void setupDescriptorPool()
	{
		std::vector<VkDescriptorPoolSize> poolSizes =
		{
      vik::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2),
      vik::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2)
		};

		VkDescriptorPoolCreateInfo descriptorPoolInfo = 
      vik::initializers::descriptorPoolCreateInfo(
				poolSizes.size(),
				poolSizes.data(),
				2);

    vik_log_check(vkCreateDescriptorPool(renderer->device, &descriptorPoolInfo, nullptr, &renderer->descriptorPool));
	}

	void setupDescriptorSetLayout()
	{
		std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = 
		{
			// Binding 0 : Vertex shader uniform buffer
      vik::initializers::descriptorSetLayoutBinding(
				VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 
				VK_SHADER_STAGE_VERTEX_BIT, 
				0),
			// Binding 1 : Fragment shader image sampler
      vik::initializers::descriptorSetLayoutBinding(
				VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 
				VK_SHADER_STAGE_FRAGMENT_BIT, 
				1)
		};

		VkDescriptorSetLayoutCreateInfo descriptorLayout = 
      vik::initializers::descriptorSetLayoutCreateInfo(
				setLayoutBindings.data(),
				setLayoutBindings.size());

    vik_log_check(vkCreateDescriptorSetLayout(renderer->device, &descriptorLayout, nullptr, &descriptorSetLayout));

		VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo =
      vik::initializers::pipelineLayoutCreateInfo(
				&descriptorSetLayout,
				1);

    vik_log_check(vkCreatePipelineLayout(renderer->device, &pPipelineLayoutCreateInfo, nullptr, &pipelineLayout));
	}

	void setupDescriptorSets()
	{
		// Image descriptor for the cube map texture
		VkDescriptorImageInfo textureDescriptor =
      vik::initializers::descriptorImageInfo(
				cubeMap.sampler,
				cubeMap.view,
				cubeMap.imageLayout);

		VkDescriptorSetAllocateInfo allocInfo =
      vik::initializers::descriptorSetAllocateInfo(
        renderer->descriptorPool,
				&descriptorSetLayout,
				1);

		// 3D object descriptor set
    vik_log_check(vkAllocateDescriptorSets(renderer->device, &allocInfo, &descriptorSets.object));

		std::vector<VkWriteDescriptorSet> writeDescriptorSets =
		{
			// Binding 0 : Vertex shader uniform buffer
      vik::initializers::writeDescriptorSet(
				descriptorSets.object, 
				VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 
				0, 
				&uniformBuffers.object.descriptor),
			// Binding 1 : Fragment shader cubemap sampler
      vik::initializers::writeDescriptorSet(
				descriptorSets.object, 
				VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 
				1, 
				&textureDescriptor)
		};
    vkUpdateDescriptorSets(renderer->device, writeDescriptorSets.size(), writeDescriptorSets.data(), 0, NULL);

		// Sky box descriptor set
    vik_log_check(vkAllocateDescriptorSets(renderer->device, &allocInfo, &descriptorSets.skybox));

		writeDescriptorSets =
		{
			// Binding 0 : Vertex shader uniform buffer
      vik::initializers::writeDescriptorSet(
				descriptorSets.skybox,
				VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				0,
				&uniformBuffers.skybox.descriptor),
			// Binding 1 : Fragment shader cubemap sampler
      vik::initializers::writeDescriptorSet(
				descriptorSets.skybox,
				VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				1,
				&textureDescriptor)
		};
    vkUpdateDescriptorSets(renderer->device, writeDescriptorSets.size(), writeDescriptorSets.data(), 0, NULL);
	}

	void preparePipelines()
	{
		VkPipelineInputAssemblyStateCreateInfo inputAssemblyState =
      vik::initializers::pipelineInputAssemblyStateCreateInfo(
				VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
				0,
				VK_FALSE);

		VkPipelineRasterizationStateCreateInfo rasterizationState =
      vik::initializers::pipelineRasterizationStateCreateInfo(
				VK_POLYGON_MODE_FILL,
				VK_CULL_MODE_BACK_BIT,
				VK_FRONT_FACE_COUNTER_CLOCKWISE,
				0);

		VkPipelineColorBlendAttachmentState blendAttachmentState =
      vik::initializers::pipelineColorBlendAttachmentState(
				0xf,
				VK_FALSE);

		VkPipelineColorBlendStateCreateInfo colorBlendState =
      vik::initializers::pipelineColorBlendStateCreateInfo(
				1, 
				&blendAttachmentState);

		VkPipelineDepthStencilStateCreateInfo depthStencilState =
      vik::initializers::pipelineDepthStencilStateCreateInfo(
				VK_FALSE,
				VK_FALSE,
				VK_COMPARE_OP_LESS_OR_EQUAL);

		VkPipelineViewportStateCreateInfo viewportState =
      vik::initializers::pipelineViewportStateCreateInfo(1, 1, 0);

		VkPipelineMultisampleStateCreateInfo multisampleState =
      vik::initializers::pipelineMultisampleStateCreateInfo(
				VK_SAMPLE_COUNT_1_BIT,
				0);

		std::vector<VkDynamicState> dynamicStateEnables = {
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR
		};
		VkPipelineDynamicStateCreateInfo dynamicState =
      vik::initializers::pipelineDynamicStateCreateInfo(
				dynamicStateEnables.data(),
				dynamicStateEnables.size(),
				0);

		// Skybox pipeline (background cube)
		std::array<VkPipelineShaderStageCreateInfo,2> shaderStages;

    shaderStages[0] = vik::Shader::load(renderer->device, "cubemap/skybox.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = vik::Shader::load(renderer->device, "cubemap/skybox.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

		VkGraphicsPipelineCreateInfo pipelineCreateInfo =
      vik::initializers::pipelineCreateInfo(
				pipelineLayout,
        renderer->render_pass,
				0);

		pipelineCreateInfo.pVertexInputState = &vertices.inputState;
		pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
		pipelineCreateInfo.pRasterizationState = &rasterizationState;
		pipelineCreateInfo.pColorBlendState = &colorBlendState;
		pipelineCreateInfo.pMultisampleState = &multisampleState;
		pipelineCreateInfo.pViewportState = &viewportState;
		pipelineCreateInfo.pDepthStencilState = &depthStencilState;
		pipelineCreateInfo.pDynamicState = &dynamicState;
		pipelineCreateInfo.stageCount = shaderStages.size();
		pipelineCreateInfo.pStages = shaderStages.data();

    vik_log_check(vkCreateGraphicsPipelines(renderer->device, renderer->pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipelines.skybox));

		// Cube map reflect pipeline
    shaderStages[0] = vik::Shader::load(renderer->device, "cubemap/reflect.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = vik::Shader::load(renderer->device, "cubemap/reflect.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		// Enable depth test and write
		depthStencilState.depthWriteEnable = VK_TRUE;
		depthStencilState.depthTestEnable = VK_TRUE;
		// Flip cull mode
		rasterizationState.cullMode = VK_CULL_MODE_FRONT_BIT;
    vik_log_check(vkCreateGraphicsPipelines(renderer->device, renderer->pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipelines.reflect));
	}

	// Prepare and initialize uniform buffer containing shader uniforms
	void prepareUniformBuffers()
	{
		// Objact vertex shader uniform buffer
    vik_log_check(renderer->vksDevice->createBuffer(
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&uniformBuffers.object,
			sizeof(uboVS)));

		// Skybox vertex shader uniform buffer
    vik_log_check(renderer->vksDevice->createBuffer(
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&uniformBuffers.skybox,
			sizeof(uboVS)));

		// Map persistent
    vik_log_check(uniformBuffers.object.map());
    vik_log_check(uniformBuffers.skybox.map());

		updateUniformBuffers();
	}

	void updateUniformBuffers()
	{
		// 3D object
		glm::mat4 viewMatrix = glm::mat4();
    uboVS.projection = glm::perspective(glm::radians(60.0f), (float)renderer->width / (float)renderer->height, 0.001f, 256.0f);
    viewMatrix = glm::translate(viewMatrix, glm::vec3(0.0f, 0.0f, camera->zoom));

    vik_log_d("%.2f [%.2f, %.2f, %.2f]",
              camera->cameraPos,
              camera->rotation.x,
              camera->rotation.y,
              camera->rotation.z);

		uboVS.model = glm::mat4();
    uboVS.model = viewMatrix * glm::translate(uboVS.model, camera->cameraPos);
    uboVS.model = glm::rotate(uboVS.model, glm::radians(camera->rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
    uboVS.model = glm::rotate(uboVS.model, glm::radians(camera->rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
    uboVS.model = glm::rotate(uboVS.model, glm::radians(camera->rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

		memcpy(uniformBuffers.object.mapped, &uboVS, sizeof(uboVS));

		// Skybox
		viewMatrix = glm::mat4();
    uboVS.projection = glm::perspective(glm::radians(60.0f), (float)renderer->width / (float)renderer->height, 0.001f, 256.0f);

		uboVS.model = glm::mat4();
		uboVS.model = viewMatrix * glm::translate(uboVS.model, glm::vec3(0, 0, 0));
    uboVS.model = glm::rotate(uboVS.model, glm::radians(camera->rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
    uboVS.model = glm::rotate(uboVS.model, glm::radians(camera->rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
    uboVS.model = glm::rotate(uboVS.model, glm::radians(camera->rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

		memcpy(uniformBuffers.skybox.mapped, &uboVS, sizeof(uboVS));
	}

	void draw()
	{

    VkSubmitInfo submit_info = renderer->init_render_submit_info();

    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &renderer->cmd_buffers[renderer->currentBuffer];
    vik_log_check(vkQueueSubmit(renderer->queue, 1, &submit_info, VK_NULL_HANDLE));

	}

  void init()
	{
    vik::Application::init();
		loadTextures();
		loadMeshes();
		setupVertexDescriptions();
		prepareUniformBuffers();
		setupDescriptorSetLayout();
		preparePipelines();
		setupDescriptorPool();
		setupDescriptorSets();
		buildCommandBuffers();
	}

	virtual void render()
	{
		draw();
	}

  virtual void view_changed_cb()
	{
		updateUniformBuffers();
	}

	void toggleSkyBox()
	{
		displaySkybox = !displaySkybox;
		reBuildCommandBuffers();
	}

	void toggleObject()
	{
		models.objectIndex++;
		if (models.objectIndex >= static_cast<uint32_t>(models.objects.size()))
		{
			models.objectIndex = 0;
		}
		reBuildCommandBuffers();
	}

	void changeLodBias(float delta)
	{
		uboVS.lodBias += delta;
		if (uboVS.lodBias < 0.0f)
		{
			uboVS.lodBias = 0.0f;
		}
		if (uboVS.lodBias > cubeMap.mipLevels)
		{
			uboVS.lodBias = cubeMap.mipLevels;
		}
		updateUniformBuffers();
    //TODO
    //renderer->updateTextOverlay();
	}

  void key_pressed(vik::Input::Key key) {
    switch (key) {
      case vik::Input::Key::S:
        toggleSkyBox();
        break;
      case vik::Input::Key::SPACE:
        toggleObject();
        break;
      case vik::Input::Key::KPPLUS:
        changeLodBias(0.1f);
        break;
      case vik::Input::Key::KPMINUS:
        changeLodBias(-0.1f);
        break;
    }
  }

  virtual void update_text_overlay(vik::TextOverlay *overlay)
	{
		std::stringstream ss;
		ss << std::setprecision(2) << std::fixed << uboVS.lodBias;
    overlay->addText("Press \"s\" to toggle skybox", 5.0f, 85.0f, vik::TextOverlay::alignLeft);
    overlay->addText("Press \"space\" to toggle object", 5.0f, 100.0f, vik::TextOverlay::alignLeft);
    overlay->addText("LOD bias: " + ss.str() + " (numpad +/- to change)", 5.0f, 115.0f, vik::TextOverlay::alignLeft);
	}
};

int main(int argc, char *argv[]) {
  XRCubeMap app(argc, argv);
  app.init();
  app.loop();
  return 0;
}
