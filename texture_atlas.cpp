#include <dirent.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "./libs/stb_image_write.h"

    struct TextureAtlas {
        Texture texture;

        //NOTE: Hash table of assets
        AtlasAsset *items[4096];
    };


    AtlasAsset *textureAtlas_addItem(TextureAtlas *atlas, char *name, float4 uv) {
        uint32_t hash = get_crc32_for_string(name);

        hash %= arrayCount(atlas->items);
        assert(hash < arrayCount(atlas->items));

        AtlasAsset **aPtr = &atlas->items[hash];

        while(*aPtr) {
            aPtr = &((*aPtr)->next);
        }

        assert((*aPtr) == 0);

        AtlasAsset *a = pushStruct(&globalLongTermArena, AtlasAsset);

        a->name = name;
        a->uv = uv;
        a->next = 0;

        *aPtr = a;

        return a;
    }

    AtlasAsset *textureAtlas_getItem(TextureAtlas *atlas, char *name) {
        AtlasAsset *result = 0;

        uint32_t hash = get_crc32_for_string(name);

        uint32_t hashIndex = hash % arrayCount(atlas->items);
        assert(hashIndex < arrayCount(atlas->items));

        AtlasAsset *a = atlas->items[hashIndex];

        while(a && !result) {
            uint32_t hashText = get_crc32_for_string(a->name);
            if(hashText == hash && easyString_stringsMatch_nullTerminated(a->name, name)) {
                result = a;
            }

            a = a->next;
        }

        return result;
    }

    // void DEBUG_runUnitTests() {
    //     TextureAtlas atlas = {};

    //     AtlasAsset *a = addItem(&atlas, "name", make_float4(1, 1, 1, 1));
    //     AtlasAsset *b = getItem(&atlas, "name", 0);

    //     AtlasAsset *c = addItem(&atlas, "name", make_float4(1, 0, 0, 1));
    //     AtlasAsset *d = getItem(&atlas, "name", 1);

    //     AtlasAsset *e = getItem(&atlas, "name1", 1);


    //     assert(a);
    //     assert(b);
    //     assert(c);

    //     assert(a == b);
    //     assert(c == d);
    //     assert(a != d);

    //     assert(!e);

    // }
 
    void createTextureAtlas(Renderer *renderer, char *folder) {
        char *imgFileTypes[] = {"jpg", "jpeg", "png", "bmp", "PNG"};
        FileNameOfType files = getDirectoryFilesOfType(folder, imgFileTypes, arrayCount(imgFileTypes));

        float outputW = TEXTURE_ATLAS_DIM;
        float outputH = TEXTURE_ATLAS_DIM;

        int margin = 4;

        float xAt = margin;
        float yAt = -margin;

        float16 screenGuiT = make_ortho_matrix_top_left_corner(outputW, outputH, MATH_3D_NEAR_CLIP_PlANE, MATH_3D_FAR_CLIP_PlANE);
        FrameBuffer frameBuffer = createFrameBuffer(outputW, outputH);
        rendererBindFrameBuffer(&frameBuffer);
        glClearColor(0, 0, 0, 0);
        renderCheckError();
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);  
        renderCheckError();

        glViewport(0, 0, outputW, outputH);

        game_file_handle atlasJsonFile = platformBeginFileWrite(TEXTURE_ATLAS_WRITE_DATA);
        assert(!atlasJsonFile.HasErrors);

        size_t offset = 0;

        float largestY = 0;
        for(int i = 0; i < files.count; ++i) {
            char *name = files.names[i];

            Texture t = loadTextureToGPU(name);

            if((xAt + t.w + margin) >= outputW) {
                xAt = margin;
                yAt -= largestY;
                largestY = 0;
            }

            pushAtlasQuad_(renderer, make_float3(xAt + 0.5f*t.w, yAt - 0.5f*t.h, 1), make_float3(t.w, t.h, 1), make_float3(0, 0, 0), make_float4(0, 1, 0, 1), make_float4(1, 1, 1, 1), true);

            //NOTE: Draw the texture at the coord
            if(renderer->atlasQuadHUDCount > 0) {
                TimeOfDayValues timeOfDayValues;
                //NOTE: Draw circle oultines
                updateInstanceData(renderer->quadModel.instanceBufferhandle, renderer->atlasHUDQuads, renderer->atlasQuadHUDCount*sizeof(InstanceDataWithRotation));
                drawModels(renderer, &renderer->quadModel, &renderer->quadTextureShader, t.handle, renderer->atlasQuadHUDCount, screenGuiT, screenGuiT, float16_identity(), make_float3(0, 0, 0), false, timeOfDayValues);

                renderer->atlasQuadHUDCount = 0;
            }

            char *strToWrite = easy_createString_printf(&globalPerFrameArena, "{\"name\": \"%s\", \"uv\": %f %f %f %f}\n", getFileLastPortionWithArena(name, &globalPerFrameArena), (double)xAt / (double)outputW, (double)-1*yAt / (double)outputH, (double)(xAt + t.w) / (double)outputW, (double)(-1*yAt + t.h) / (double)outputH);

            offset = platformWriteFile(&atlasJsonFile, strToWrite, easyString_getSizeInBytes_utf8(strToWrite), offset);

            renderer->alphaItemCount = 0;

            if(largestY < (t.h + margin)) {
                largestY = (t.h + margin);
            }

            xAt += (t.w + margin);

            if(yAt >= outputH) {
                assert(false);
                //NOTE: Out of room
            }
        }

        platformEndFile(atlasJsonFile);

        //NOTE: Flush the openGL command calls
        glFlush();

        //NOTE: Save the buffer to a file
        size_t bytesPerPixel = sizeof(uint8_t)*4;
        size_t sizeToAlloc = outputW*outputH*bytesPerPixel;
        int stride_in_bytes = bytesPerPixel*outputW;

        uint8_t *pixelBuffer = (uint8_t *)calloc(sizeToAlloc, 1);

        glReadPixels(0, 0,
                            outputW,
                            outputH,
                            GL_RGBA,
                            GL_UNSIGNED_BYTE,
                            pixelBuffer);


        stbi_flip_vertically_on_write(1);
        int writeResult = stbi_write_png(TEXTURE_ATLAS_WRITE_IMAGE, outputW, outputH, 4, pixelBuffer, stride_in_bytes);

        free(pixelBuffer);

        deleteFrameBuffer(&frameBuffer);
        frameBuffer.handle = 0;
        rendererBindFrameBuffer(&frameBuffer);

    }

    TextureAtlas readTextureAtlas(char *jsonFileName, char *textureFileName) {
        TextureAtlas result = {};

        FileContents contents = getFileContentsNullTerminate(jsonFileName);
        assert(contents.valid);
        assert(contents.fileSize > 0);
        assert(contents.memory);

        EasyTokenizer tokenizer = lexBeginParsing(contents.memory, EASY_LEX_OPTION_EAT_WHITE_SPACE);

        bool parsing = true;
        while(parsing) {
            EasyToken t = lexGetNextToken(&tokenizer);

            if(t.type == TOKEN_NULL_TERMINATOR) {
                parsing = false;
            } else if(t.type == TOKEN_OPEN_BRACKET) {
                //NOTE: Get the item out
                float4 uv = make_float4(0, 0, 0, 0);

                t = lexGetNextToken(&tokenizer);
                assert(t.type == TOKEN_STRING);
                t = lexGetNextToken(&tokenizer);
                assert(t.type == TOKEN_COLON);
                t = lexGetNextToken(&tokenizer);
                assert(t.type == TOKEN_STRING);
                char *assetName = easyString_copyToArena_(t.at, &globalLongTermArena, t.size);
                t = lexGetNextToken(&tokenizer);
                assert(t.type == TOKEN_COMMA);
                t = lexGetNextToken(&tokenizer);
                assert(t.type == TOKEN_STRING);
                t = lexGetNextToken(&tokenizer);
                assert(t.type == TOKEN_COLON);
                t = lexGetNextToken(&tokenizer);
                assert(t.type == TOKEN_FLOAT);
                uv.x = t.floatVal;
                t = lexGetNextToken(&tokenizer);
                assert(t.type == TOKEN_FLOAT);
                uv.y = t.floatVal;
                t = lexGetNextToken(&tokenizer);
                assert(t.type == TOKEN_FLOAT);
                uv.z = t.floatVal;
                t = lexGetNextToken(&tokenizer);
                assert(t.type == TOKEN_FLOAT);
                uv.w = t.floatVal;

                textureAtlas_addItem(&result, assetName, uv);
            }
        }

        result.texture = loadTextureToGPU(textureFileName);

        return result;
    }