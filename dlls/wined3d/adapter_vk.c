/*
 * Copyright 2018 Henri Verbeet for CodeWeavers
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include "config.h"
#include "wine/port.h"
#include "wined3d_private.h"

#include "wine/vulkan_driver.h"

WINE_DEFAULT_DEBUG_CHANNEL(d3d);

static const struct wined3d_state_entry_template misc_state_template_vk[] =
{
    {STATE_CONSTANT_BUFFER(WINED3D_SHADER_TYPE_VERTEX),   {STATE_CONSTANT_BUFFER(WINED3D_SHADER_TYPE_VERTEX),   state_nop}},
    {STATE_CONSTANT_BUFFER(WINED3D_SHADER_TYPE_HULL),     {STATE_CONSTANT_BUFFER(WINED3D_SHADER_TYPE_HULL),     state_nop}},
    {STATE_CONSTANT_BUFFER(WINED3D_SHADER_TYPE_DOMAIN),   {STATE_CONSTANT_BUFFER(WINED3D_SHADER_TYPE_DOMAIN),   state_nop}},
    {STATE_CONSTANT_BUFFER(WINED3D_SHADER_TYPE_GEOMETRY), {STATE_CONSTANT_BUFFER(WINED3D_SHADER_TYPE_GEOMETRY), state_nop}},
    {STATE_CONSTANT_BUFFER(WINED3D_SHADER_TYPE_PIXEL),    {STATE_CONSTANT_BUFFER(WINED3D_SHADER_TYPE_PIXEL),    state_nop}},
    {STATE_CONSTANT_BUFFER(WINED3D_SHADER_TYPE_COMPUTE),  {STATE_CONSTANT_BUFFER(WINED3D_SHADER_TYPE_COMPUTE),  state_nop}},
    {STATE_GRAPHICS_SHADER_RESOURCE_BINDING,              {STATE_GRAPHICS_SHADER_RESOURCE_BINDING,              state_nop}},
    {STATE_GRAPHICS_UNORDERED_ACCESS_VIEW_BINDING,        {STATE_GRAPHICS_UNORDERED_ACCESS_VIEW_BINDING,        state_nop}},
    {STATE_COMPUTE_SHADER_RESOURCE_BINDING,               {STATE_COMPUTE_SHADER_RESOURCE_BINDING,               state_nop}},
    {STATE_COMPUTE_UNORDERED_ACCESS_VIEW_BINDING,         {STATE_COMPUTE_UNORDERED_ACCESS_VIEW_BINDING,         state_nop}},
    {STATE_STREAM_OUTPUT,                                 {STATE_STREAM_OUTPUT,                                 state_nop}},
    {STATE_BLEND,                                         {STATE_BLEND,                                         state_nop}},
    {STATE_BLEND_FACTOR,                                  {STATE_BLEND_FACTOR,                                  state_nop}},
    {STATE_STREAMSRC,                                     {STATE_STREAMSRC,                                     state_nop}},
    {STATE_VDECL,                                         {STATE_VDECL,                                         state_nop}},
    {STATE_RASTERIZER,                                    {STATE_RASTERIZER,                                    state_nop}},
    {STATE_SCISSORRECT,                                   {STATE_SCISSORRECT,                                   state_nop}},
    {STATE_POINTSPRITECOORDORIGIN,                        {STATE_POINTSPRITECOORDORIGIN,                        state_nop}},

    {STATE_TEXTURESTAGE(0, WINED3D_TSS_BUMPENV_MAT00),    {STATE_TEXTURESTAGE(0, WINED3D_TSS_BUMPENV_MAT00),    state_nop}},
    {STATE_TEXTURESTAGE(0, WINED3D_TSS_BUMPENV_MAT01),    {STATE_TEXTURESTAGE(0, WINED3D_TSS_BUMPENV_MAT00)}},
    {STATE_TEXTURESTAGE(0, WINED3D_TSS_BUMPENV_MAT10),    {STATE_TEXTURESTAGE(0, WINED3D_TSS_BUMPENV_MAT00)}},
    {STATE_TEXTURESTAGE(0, WINED3D_TSS_BUMPENV_MAT11),    {STATE_TEXTURESTAGE(0, WINED3D_TSS_BUMPENV_MAT00)}},
    {STATE_TEXTURESTAGE(1, WINED3D_TSS_BUMPENV_MAT00),    {STATE_TEXTURESTAGE(1, WINED3D_TSS_BUMPENV_MAT00),    state_nop}},
    {STATE_TEXTURESTAGE(1, WINED3D_TSS_BUMPENV_MAT01),    {STATE_TEXTURESTAGE(1, WINED3D_TSS_BUMPENV_MAT00)}},
    {STATE_TEXTURESTAGE(1, WINED3D_TSS_BUMPENV_MAT10),    {STATE_TEXTURESTAGE(1, WINED3D_TSS_BUMPENV_MAT00)}},
    {STATE_TEXTURESTAGE(1, WINED3D_TSS_BUMPENV_MAT11),    {STATE_TEXTURESTAGE(1, WINED3D_TSS_BUMPENV_MAT00)}},
    {STATE_TEXTURESTAGE(2, WINED3D_TSS_BUMPENV_MAT00),    {STATE_TEXTURESTAGE(2, WINED3D_TSS_BUMPENV_MAT00),    state_nop}},
    {STATE_TEXTURESTAGE(2, WINED3D_TSS_BUMPENV_MAT01),    {STATE_TEXTURESTAGE(2, WINED3D_TSS_BUMPENV_MAT00)}},
    {STATE_TEXTURESTAGE(2, WINED3D_TSS_BUMPENV_MAT10),    {STATE_TEXTURESTAGE(2, WINED3D_TSS_BUMPENV_MAT00)}},
    {STATE_TEXTURESTAGE(2, WINED3D_TSS_BUMPENV_MAT11),    {STATE_TEXTURESTAGE(2, WINED3D_TSS_BUMPENV_MAT00)}},
    {STATE_TEXTURESTAGE(3, WINED3D_TSS_BUMPENV_MAT00),    {STATE_TEXTURESTAGE(3, WINED3D_TSS_BUMPENV_MAT00),    state_nop}},
    {STATE_TEXTURESTAGE(3, WINED3D_TSS_BUMPENV_MAT01),    {STATE_TEXTURESTAGE(3, WINED3D_TSS_BUMPENV_MAT00)}},
    {STATE_TEXTURESTAGE(3, WINED3D_TSS_BUMPENV_MAT10),    {STATE_TEXTURESTAGE(3, WINED3D_TSS_BUMPENV_MAT00)}},
    {STATE_TEXTURESTAGE(3, WINED3D_TSS_BUMPENV_MAT11),    {STATE_TEXTURESTAGE(3, WINED3D_TSS_BUMPENV_MAT00)}},
    {STATE_TEXTURESTAGE(4, WINED3D_TSS_BUMPENV_MAT00),    {STATE_TEXTURESTAGE(4, WINED3D_TSS_BUMPENV_MAT00),    state_nop}},
    {STATE_TEXTURESTAGE(4, WINED3D_TSS_BUMPENV_MAT01),    {STATE_TEXTURESTAGE(4, WINED3D_TSS_BUMPENV_MAT00)}},
    {STATE_TEXTURESTAGE(4, WINED3D_TSS_BUMPENV_MAT10),    {STATE_TEXTURESTAGE(4, WINED3D_TSS_BUMPENV_MAT00)}},
    {STATE_TEXTURESTAGE(4, WINED3D_TSS_BUMPENV_MAT11),    {STATE_TEXTURESTAGE(4, WINED3D_TSS_BUMPENV_MAT00)}},
    {STATE_TEXTURESTAGE(5, WINED3D_TSS_BUMPENV_MAT00),    {STATE_TEXTURESTAGE(5, WINED3D_TSS_BUMPENV_MAT00),    state_nop}},
    {STATE_TEXTURESTAGE(5, WINED3D_TSS_BUMPENV_MAT01),    {STATE_TEXTURESTAGE(5, WINED3D_TSS_BUMPENV_MAT00)}},
    {STATE_TEXTURESTAGE(5, WINED3D_TSS_BUMPENV_MAT10),    {STATE_TEXTURESTAGE(5, WINED3D_TSS_BUMPENV_MAT00)}},
    {STATE_TEXTURESTAGE(5, WINED3D_TSS_BUMPENV_MAT11),    {STATE_TEXTURESTAGE(5, WINED3D_TSS_BUMPENV_MAT00)}},
    {STATE_TEXTURESTAGE(6, WINED3D_TSS_BUMPENV_MAT00),    {STATE_TEXTURESTAGE(6, WINED3D_TSS_BUMPENV_MAT00),    state_nop}},
    {STATE_TEXTURESTAGE(6, WINED3D_TSS_BUMPENV_MAT01),    {STATE_TEXTURESTAGE(6, WINED3D_TSS_BUMPENV_MAT00)}},
    {STATE_TEXTURESTAGE(6, WINED3D_TSS_BUMPENV_MAT10),    {STATE_TEXTURESTAGE(6, WINED3D_TSS_BUMPENV_MAT00)}},
    {STATE_TEXTURESTAGE(6, WINED3D_TSS_BUMPENV_MAT11),    {STATE_TEXTURESTAGE(6, WINED3D_TSS_BUMPENV_MAT00)}},
    {STATE_TEXTURESTAGE(7, WINED3D_TSS_BUMPENV_MAT00),    {STATE_TEXTURESTAGE(7, WINED3D_TSS_BUMPENV_MAT00),    state_nop}},
    {STATE_TEXTURESTAGE(7, WINED3D_TSS_BUMPENV_MAT01),    {STATE_TEXTURESTAGE(7, WINED3D_TSS_BUMPENV_MAT00)}},
    {STATE_TEXTURESTAGE(7, WINED3D_TSS_BUMPENV_MAT10),    {STATE_TEXTURESTAGE(7, WINED3D_TSS_BUMPENV_MAT00)}},
    {STATE_TEXTURESTAGE(7, WINED3D_TSS_BUMPENV_MAT11),    {STATE_TEXTURESTAGE(7, WINED3D_TSS_BUMPENV_MAT00)}},
    {STATE_TEXTURESTAGE(0, WINED3D_TSS_BUMPENV_LSCALE),   {STATE_TEXTURESTAGE(0, WINED3D_TSS_BUMPENV_LSCALE),   state_nop}},
    {STATE_TEXTURESTAGE(0, WINED3D_TSS_BUMPENV_LOFFSET),  {STATE_TEXTURESTAGE(0, WINED3D_TSS_BUMPENV_LSCALE)}},
    {STATE_TEXTURESTAGE(1, WINED3D_TSS_BUMPENV_LSCALE),   {STATE_TEXTURESTAGE(1, WINED3D_TSS_BUMPENV_LSCALE),   state_nop}},
    {STATE_TEXTURESTAGE(1, WINED3D_TSS_BUMPENV_LOFFSET),  {STATE_TEXTURESTAGE(1, WINED3D_TSS_BUMPENV_LSCALE)}},
    {STATE_TEXTURESTAGE(2, WINED3D_TSS_BUMPENV_LSCALE),   {STATE_TEXTURESTAGE(2, WINED3D_TSS_BUMPENV_LSCALE),   state_nop}},
    {STATE_TEXTURESTAGE(2, WINED3D_TSS_BUMPENV_LOFFSET),  {STATE_TEXTURESTAGE(2, WINED3D_TSS_BUMPENV_LSCALE)}},
    {STATE_TEXTURESTAGE(3, WINED3D_TSS_BUMPENV_LSCALE),   {STATE_TEXTURESTAGE(3, WINED3D_TSS_BUMPENV_LSCALE),   state_nop}},
    {STATE_TEXTURESTAGE(3, WINED3D_TSS_BUMPENV_LOFFSET),  {STATE_TEXTURESTAGE(3, WINED3D_TSS_BUMPENV_LSCALE)}},
    {STATE_TEXTURESTAGE(4, WINED3D_TSS_BUMPENV_LSCALE),   {STATE_TEXTURESTAGE(4, WINED3D_TSS_BUMPENV_LSCALE),   state_nop}},
    {STATE_TEXTURESTAGE(4, WINED3D_TSS_BUMPENV_LOFFSET),  {STATE_TEXTURESTAGE(4, WINED3D_TSS_BUMPENV_LSCALE)}},
    {STATE_TEXTURESTAGE(5, WINED3D_TSS_BUMPENV_LSCALE),   {STATE_TEXTURESTAGE(5, WINED3D_TSS_BUMPENV_LSCALE),   state_nop}},
    {STATE_TEXTURESTAGE(5, WINED3D_TSS_BUMPENV_LOFFSET),  {STATE_TEXTURESTAGE(5, WINED3D_TSS_BUMPENV_LSCALE)}},
    {STATE_TEXTURESTAGE(6, WINED3D_TSS_BUMPENV_LSCALE),   {STATE_TEXTURESTAGE(6, WINED3D_TSS_BUMPENV_LSCALE),   state_nop}},
    {STATE_TEXTURESTAGE(6, WINED3D_TSS_BUMPENV_LOFFSET),  {STATE_TEXTURESTAGE(6, WINED3D_TSS_BUMPENV_LSCALE)}},
    {STATE_TEXTURESTAGE(7, WINED3D_TSS_BUMPENV_LSCALE),   {STATE_TEXTURESTAGE(7, WINED3D_TSS_BUMPENV_LSCALE),   state_nop}},
    {STATE_TEXTURESTAGE(7, WINED3D_TSS_BUMPENV_LOFFSET),  {STATE_TEXTURESTAGE(7, WINED3D_TSS_BUMPENV_LSCALE)}},

    {STATE_VIEWPORT,                                      {STATE_VIEWPORT,                                      state_nop}},
    {STATE_INDEXBUFFER,                                   {STATE_INDEXBUFFER,                                   state_nop}},
    {STATE_RENDER(WINED3D_RS_ANTIALIAS),                  {STATE_RENDER(WINED3D_RS_ANTIALIAS),                  state_nop}},
    {STATE_RENDER(WINED3D_RS_TEXTUREPERSPECTIVE),         {STATE_RENDER(WINED3D_RS_TEXTUREPERSPECTIVE),         state_nop}},
    {STATE_RENDER(WINED3D_RS_ZENABLE),                    {STATE_RENDER(WINED3D_RS_ZENABLE),                    state_nop}},
    {STATE_RENDER(WINED3D_RS_WRAPU),                      {STATE_RENDER(WINED3D_RS_WRAPU),                      state_nop}},
    {STATE_RENDER(WINED3D_RS_WRAPV),                      {STATE_RENDER(WINED3D_RS_WRAPV),                      state_nop}},
    {STATE_RENDER(WINED3D_RS_LINEPATTERN),                {STATE_RENDER(WINED3D_RS_LINEPATTERN),                state_nop}},
    {STATE_RENDER(WINED3D_RS_MONOENABLE),                 {STATE_RENDER(WINED3D_RS_MONOENABLE),                 state_nop}},
    {STATE_RENDER(WINED3D_RS_ROP2),                       {STATE_RENDER(WINED3D_RS_ROP2),                       state_nop}},
    {STATE_RENDER(WINED3D_RS_PLANEMASK),                  {STATE_RENDER(WINED3D_RS_PLANEMASK),                  state_nop}},
    {STATE_RENDER(WINED3D_RS_ZWRITEENABLE),               {STATE_RENDER(WINED3D_RS_ZENABLE)}},
    {STATE_RENDER(WINED3D_RS_LASTPIXEL),                  {STATE_RENDER(WINED3D_RS_LASTPIXEL),                  state_nop}},
    {STATE_RENDER(WINED3D_RS_ZFUNC),                      {STATE_RENDER(WINED3D_RS_ZENABLE)}},
    {STATE_RENDER(WINED3D_RS_DITHERENABLE),               {STATE_RENDER(WINED3D_RS_DITHERENABLE),               state_nop}},
    {STATE_RENDER(WINED3D_RS_SUBPIXEL),                   {STATE_RENDER(WINED3D_RS_SUBPIXEL),                   state_nop}},
    {STATE_RENDER(WINED3D_RS_SUBPIXELX),                  {STATE_RENDER(WINED3D_RS_SUBPIXELX),                  state_nop}},
    {STATE_RENDER(WINED3D_RS_STIPPLEDALPHA),              {STATE_RENDER(WINED3D_RS_STIPPLEDALPHA),              state_nop}},
    {STATE_RENDER(WINED3D_RS_STIPPLEENABLE),              {STATE_RENDER(WINED3D_RS_STIPPLEENABLE),              state_nop}},
    {STATE_RENDER(WINED3D_RS_MIPMAPLODBIAS),              {STATE_RENDER(WINED3D_RS_MIPMAPLODBIAS),              state_nop}},
    {STATE_RENDER(WINED3D_RS_ANISOTROPY),                 {STATE_RENDER(WINED3D_RS_ANISOTROPY),                 state_nop}},
    {STATE_RENDER(WINED3D_RS_FLUSHBATCH),                 {STATE_RENDER(WINED3D_RS_FLUSHBATCH),                 state_nop}},
    {STATE_RENDER(WINED3D_RS_TRANSLUCENTSORTINDEPENDENT), {STATE_RENDER(WINED3D_RS_TRANSLUCENTSORTINDEPENDENT), state_nop}},
    {STATE_RENDER(WINED3D_RS_STENCILENABLE),              {STATE_RENDER(WINED3D_RS_ZENABLE)}},
    {STATE_RENDER(WINED3D_RS_STENCILFAIL),                {STATE_RENDER(WINED3D_RS_ZENABLE)}},
    {STATE_RENDER(WINED3D_RS_STENCILZFAIL),               {STATE_RENDER(WINED3D_RS_ZENABLE)}},
    {STATE_RENDER(WINED3D_RS_STENCILPASS),                {STATE_RENDER(WINED3D_RS_ZENABLE)}},
    {STATE_RENDER(WINED3D_RS_STENCILFUNC),                {STATE_RENDER(WINED3D_RS_ZENABLE)}},
    {STATE_RENDER(WINED3D_RS_STENCILREF),                 {STATE_RENDER(WINED3D_RS_ZENABLE)}},
    {STATE_RENDER(WINED3D_RS_STENCILMASK),                {STATE_RENDER(WINED3D_RS_ZENABLE)}},
    {STATE_RENDER(WINED3D_RS_STENCILWRITEMASK),           {STATE_RENDER(WINED3D_RS_ZENABLE)}},
    {STATE_RENDER(WINED3D_RS_TWOSIDEDSTENCILMODE),        {STATE_RENDER(WINED3D_RS_ZENABLE)}},
    {STATE_RENDER(WINED3D_RS_BACK_STENCILFAIL),           {STATE_RENDER(WINED3D_RS_ZENABLE)}},
    {STATE_RENDER(WINED3D_RS_BACK_STENCILZFAIL),          {STATE_RENDER(WINED3D_RS_ZENABLE)}},
    {STATE_RENDER(WINED3D_RS_BACK_STENCILPASS),           {STATE_RENDER(WINED3D_RS_ZENABLE)}},
    {STATE_RENDER(WINED3D_RS_BACK_STENCILFUNC),           {STATE_RENDER(WINED3D_RS_ZENABLE)}},
    {STATE_RENDER(WINED3D_RS_WRAP0),                      {STATE_RENDER(WINED3D_RS_WRAP0),                      state_nop}},
    {STATE_RENDER(WINED3D_RS_WRAP1),                      {STATE_RENDER(WINED3D_RS_WRAP0)}},
    {STATE_RENDER(WINED3D_RS_WRAP2),                      {STATE_RENDER(WINED3D_RS_WRAP0)}},
    {STATE_RENDER(WINED3D_RS_WRAP3),                      {STATE_RENDER(WINED3D_RS_WRAP0)}},
    {STATE_RENDER(WINED3D_RS_WRAP4),                      {STATE_RENDER(WINED3D_RS_WRAP0)}},
    {STATE_RENDER(WINED3D_RS_WRAP5),                      {STATE_RENDER(WINED3D_RS_WRAP0)}},
    {STATE_RENDER(WINED3D_RS_WRAP6),                      {STATE_RENDER(WINED3D_RS_WRAP0)}},
    {STATE_RENDER(WINED3D_RS_WRAP7),                      {STATE_RENDER(WINED3D_RS_WRAP0)}},
    {STATE_RENDER(WINED3D_RS_WRAP8),                      {STATE_RENDER(WINED3D_RS_WRAP0)}},
    {STATE_RENDER(WINED3D_RS_WRAP9),                      {STATE_RENDER(WINED3D_RS_WRAP0)}},
    {STATE_RENDER(WINED3D_RS_WRAP10),                     {STATE_RENDER(WINED3D_RS_WRAP0)}},
    {STATE_RENDER(WINED3D_RS_WRAP11),                     {STATE_RENDER(WINED3D_RS_WRAP0)}},
    {STATE_RENDER(WINED3D_RS_WRAP12),                     {STATE_RENDER(WINED3D_RS_WRAP0)}},
    {STATE_RENDER(WINED3D_RS_WRAP13),                     {STATE_RENDER(WINED3D_RS_WRAP0)}},
    {STATE_RENDER(WINED3D_RS_WRAP14),                     {STATE_RENDER(WINED3D_RS_WRAP0)}},
    {STATE_RENDER(WINED3D_RS_WRAP15),                     {STATE_RENDER(WINED3D_RS_WRAP0)}},
    {STATE_RENDER(WINED3D_RS_EXTENTS),                    {STATE_RENDER(WINED3D_RS_EXTENTS),                    state_nop}},
    {STATE_RENDER(WINED3D_RS_COLORKEYBLENDENABLE),        {STATE_RENDER(WINED3D_RS_COLORKEYBLENDENABLE),        state_nop}},
    {STATE_RENDER(WINED3D_RS_SOFTWAREVERTEXPROCESSING),   {STATE_RENDER(WINED3D_RS_SOFTWAREVERTEXPROCESSING),   state_nop}},
    {STATE_RENDER(WINED3D_RS_PATCHEDGESTYLE),             {STATE_RENDER(WINED3D_RS_PATCHEDGESTYLE),             state_nop}},
    {STATE_RENDER(WINED3D_RS_PATCHSEGMENTS),              {STATE_RENDER(WINED3D_RS_PATCHSEGMENTS),              state_nop}},
    {STATE_RENDER(WINED3D_RS_POSITIONDEGREE),             {STATE_RENDER(WINED3D_RS_POSITIONDEGREE),             state_nop}},
    {STATE_RENDER(WINED3D_RS_NORMALDEGREE),               {STATE_RENDER(WINED3D_RS_NORMALDEGREE),               state_nop}},
    {STATE_RENDER(WINED3D_RS_MINTESSELLATIONLEVEL),       {STATE_RENDER(WINED3D_RS_ENABLEADAPTIVETESSELLATION)}},
    {STATE_RENDER(WINED3D_RS_MAXTESSELLATIONLEVEL),       {STATE_RENDER(WINED3D_RS_ENABLEADAPTIVETESSELLATION)}},
    {STATE_RENDER(WINED3D_RS_ADAPTIVETESS_X),             {STATE_RENDER(WINED3D_RS_ENABLEADAPTIVETESSELLATION)}},
    {STATE_RENDER(WINED3D_RS_ADAPTIVETESS_Y),             {STATE_RENDER(WINED3D_RS_ENABLEADAPTIVETESSELLATION)}},
    {STATE_RENDER(WINED3D_RS_ADAPTIVETESS_Z),             {STATE_RENDER(WINED3D_RS_ENABLEADAPTIVETESSELLATION)}},
    {STATE_RENDER(WINED3D_RS_ADAPTIVETESS_W),             {STATE_RENDER(WINED3D_RS_ENABLEADAPTIVETESSELLATION)}},
    {STATE_RENDER(WINED3D_RS_ENABLEADAPTIVETESSELLATION), {STATE_RENDER(WINED3D_RS_ENABLEADAPTIVETESSELLATION), state_nop}},
    {STATE_RENDER(WINED3D_RS_MULTISAMPLEANTIALIAS),       {STATE_RENDER(WINED3D_RS_MULTISAMPLEANTIALIAS),       state_nop}},
    {STATE_RENDER(WINED3D_RS_MULTISAMPLEMASK),            {STATE_RENDER(WINED3D_RS_MULTISAMPLEMASK),            state_nop}},
    {STATE_RENDER(WINED3D_RS_DEBUGMONITORTOKEN),          {STATE_RENDER(WINED3D_RS_DEBUGMONITORTOKEN),          state_nop}},
    {STATE_RENDER(WINED3D_RS_ZVISIBLE),                   {STATE_RENDER(WINED3D_RS_ZVISIBLE),                   state_nop}},
    /* Samplers */
    {STATE_SAMPLER(0),                                    {STATE_SAMPLER(0),                                    state_nop}},
    {STATE_SAMPLER(1),                                    {STATE_SAMPLER(1),                                    state_nop}},
    {STATE_SAMPLER(2),                                    {STATE_SAMPLER(2),                                    state_nop}},
    {STATE_SAMPLER(3),                                    {STATE_SAMPLER(3),                                    state_nop}},
    {STATE_SAMPLER(4),                                    {STATE_SAMPLER(4),                                    state_nop}},
    {STATE_SAMPLER(5),                                    {STATE_SAMPLER(5),                                    state_nop}},
    {STATE_SAMPLER(6),                                    {STATE_SAMPLER(6),                                    state_nop}},
    {STATE_SAMPLER(7),                                    {STATE_SAMPLER(7),                                    state_nop}},
    {STATE_SAMPLER(8),                                    {STATE_SAMPLER(8),                                    state_nop}},
    {STATE_SAMPLER(9),                                    {STATE_SAMPLER(9),                                    state_nop}},
    {STATE_SAMPLER(10),                                   {STATE_SAMPLER(10),                                   state_nop}},
    {STATE_SAMPLER(11),                                   {STATE_SAMPLER(11),                                   state_nop}},
    {STATE_SAMPLER(12),                                   {STATE_SAMPLER(12),                                   state_nop}},
    {STATE_SAMPLER(13),                                   {STATE_SAMPLER(13),                                   state_nop}},
    {STATE_SAMPLER(14),                                   {STATE_SAMPLER(14),                                   state_nop}},
    {STATE_SAMPLER(15),                                   {STATE_SAMPLER(15),                                   state_nop}},
    {STATE_SAMPLER(16), /* Vertex sampler 0 */            {STATE_SAMPLER(16),                                   state_nop}},
    {STATE_SAMPLER(17), /* Vertex sampler 1 */            {STATE_SAMPLER(17),                                   state_nop}},
    {STATE_SAMPLER(18), /* Vertex sampler 2 */            {STATE_SAMPLER(18),                                   state_nop}},
    {STATE_SAMPLER(19), /* Vertex sampler 3 */            {STATE_SAMPLER(19),                                   state_nop}},
    {STATE_BASEVERTEXINDEX,                               {STATE_STREAMSRC}},
    {STATE_FRAMEBUFFER,                                   {STATE_FRAMEBUFFER,                                   state_nop}},
    {STATE_SHADER(WINED3D_SHADER_TYPE_PIXEL),             {STATE_SHADER(WINED3D_SHADER_TYPE_PIXEL),             state_nop}},
    {STATE_SHADER(WINED3D_SHADER_TYPE_HULL),              {STATE_SHADER(WINED3D_SHADER_TYPE_HULL),              state_nop}},
    {STATE_SHADER(WINED3D_SHADER_TYPE_DOMAIN),            {STATE_SHADER(WINED3D_SHADER_TYPE_DOMAIN),            state_nop}},
    {STATE_SHADER(WINED3D_SHADER_TYPE_GEOMETRY),          {STATE_SHADER(WINED3D_SHADER_TYPE_GEOMETRY),          state_nop}},
    {STATE_SHADER(WINED3D_SHADER_TYPE_COMPUTE),           {STATE_SHADER(WINED3D_SHADER_TYPE_COMPUTE),           state_nop}},
    {0}, /* Terminate */
};

static inline const struct wined3d_adapter_vk *wined3d_adapter_vk_const(const struct wined3d_adapter *adapter)
{
    return CONTAINING_RECORD(adapter, struct wined3d_adapter_vk, a);
}

static const char *debug_vk_version(uint32_t version)
{
    return wine_dbg_sprintf("%u.%u.%u",
            VK_VERSION_MAJOR(version), VK_VERSION_MINOR(version), VK_VERSION_PATCH(version));
}

static HRESULT hresult_from_vk_result(VkResult vr)
{
    switch (vr)
    {
        case VK_SUCCESS:
            return S_OK;
        case VK_ERROR_OUT_OF_HOST_MEMORY:
            WARN("Out of host memory.\n");
            return E_OUTOFMEMORY;
        case VK_ERROR_OUT_OF_DEVICE_MEMORY:
            WARN("Out of device memory.\n");
            return E_OUTOFMEMORY;
        case VK_ERROR_DEVICE_LOST:
            WARN("Device lost.\n");
            return E_FAIL;
        case VK_ERROR_EXTENSION_NOT_PRESENT:
            WARN("Extension not present.\n");
            return E_FAIL;
        default:
            FIXME("Unhandled VkResult %d.\n", vr);
            return E_FAIL;
    }
}

#ifdef USE_WIN32_VULKAN
static BOOL wined3d_load_vulkan(struct wined3d_vk_info *vk_info)
{
    struct vulkan_ops *vk_ops = &vk_info->vk_ops;

    if (!(vk_info->vulkan_lib = LoadLibraryA("vulkan-1.dll")))
    {
        WARN("Failed to load vulkan-1.dll.\n");
        return FALSE;
    }

    vk_ops->vkGetInstanceProcAddr = (void *)GetProcAddress(vk_info->vulkan_lib, "vkGetInstanceProcAddr");
    if (!vk_ops->vkGetInstanceProcAddr)
    {
        FreeLibrary(vk_info->vulkan_lib);
        return FALSE;
    }

    return TRUE;
}

static void wined3d_unload_vulkan(struct wined3d_vk_info *vk_info)
{
    if (vk_info->vulkan_lib)
    {
        FreeLibrary(vk_info->vulkan_lib);
        vk_info->vulkan_lib = NULL;
    }
}
#else
static BOOL wined3d_load_vulkan(struct wined3d_vk_info *vk_info)
{
    struct vulkan_ops *vk_ops = &vk_info->vk_ops;
    const struct vulkan_funcs *vk_funcs;
    HDC dc;

    dc = GetDC(0);
    vk_funcs = __wine_get_vulkan_driver(dc, WINE_VULKAN_DRIVER_VERSION);
    ReleaseDC(0, dc);

    if (!vk_funcs)
        return FALSE;

    vk_ops->vkGetInstanceProcAddr = (void *)vk_funcs->p_vkGetInstanceProcAddr;
    return TRUE;
}

static void wined3d_unload_vulkan(struct wined3d_vk_info *vk_info) {}
#endif

static void adapter_vk_destroy(struct wined3d_adapter *adapter)
{
    struct wined3d_adapter_vk *adapter_vk = wined3d_adapter_vk(adapter);
    struct wined3d_vk_info *vk_info = &adapter_vk->vk_info;

    VK_CALL(vkDestroyInstance(vk_info->instance, NULL));
    wined3d_unload_vulkan(vk_info);
    wined3d_adapter_cleanup(&adapter_vk->a);
    heap_free(adapter_vk);
}

static HRESULT wined3d_select_vulkan_queue_family(const struct wined3d_adapter_vk *adapter_vk,
        uint32_t *queue_family_index, uint32_t *timestamp_bits)
{
    VkPhysicalDevice physical_device = adapter_vk->physical_device;
    const struct wined3d_vk_info *vk_info = &adapter_vk->vk_info;
    VkQueueFamilyProperties *queue_properties;
    uint32_t count, i;

    VK_CALL(vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &count, NULL));

    if (!(queue_properties = heap_calloc(count, sizeof(*queue_properties))))
        return E_OUTOFMEMORY;

    VK_CALL(vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &count, queue_properties));

    for (i = 0; i < count; ++i)
    {
        if (queue_properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            *queue_family_index = i;
            *timestamp_bits = queue_properties[i].timestampValidBits;
            heap_free(queue_properties);
            return WINED3D_OK;
        }
    }
    heap_free(queue_properties);

    WARN("Failed to find graphics queue.\n");
    return E_FAIL;
}

struct wined3d_physical_device_info
{
    VkPhysicalDeviceVertexAttributeDivisorFeaturesEXT vertex_divisor_features;

    VkPhysicalDeviceFeatures2 features2;
};

static void wined3d_disable_vulkan_features(struct wined3d_physical_device_info *info)
{
    VkPhysicalDeviceVertexAttributeDivisorFeaturesEXT *vertex_divisor_features = &info->vertex_divisor_features;
    VkPhysicalDeviceFeatures *features = &info->features2.features;

    vertex_divisor_features->vertexAttributeInstanceRateZeroDivisor = VK_FALSE;

    features->depthBounds = VK_FALSE;
    features->alphaToOne = VK_FALSE;
    features->textureCompressionETC2 = VK_FALSE;
    features->textureCompressionASTC_LDR = VK_FALSE;
    features->shaderStorageImageMultisample = VK_FALSE;
    features->shaderUniformBufferArrayDynamicIndexing = VK_FALSE;
    features->shaderSampledImageArrayDynamicIndexing = VK_FALSE;
    features->shaderStorageBufferArrayDynamicIndexing = VK_FALSE;
    features->shaderStorageImageArrayDynamicIndexing = VK_FALSE;
    features->shaderInt16 = VK_FALSE;
    features->shaderResourceResidency = VK_FALSE;
    features->shaderResourceMinLod = VK_FALSE;
    features->sparseBinding = VK_FALSE;
    features->sparseResidencyBuffer = VK_FALSE;
    features->sparseResidencyImage2D = VK_FALSE;
    features->sparseResidencyImage3D = VK_FALSE;
    features->sparseResidency2Samples = VK_FALSE;
    features->sparseResidency4Samples = VK_FALSE;
    features->sparseResidency8Samples = VK_FALSE;
    features->sparseResidency16Samples = VK_FALSE;
    features->sparseResidencyAliased = VK_FALSE;
    features->inheritedQueries = VK_FALSE;
}

static struct wined3d_allocator_chunk *wined3d_allocator_vk_create_chunk(struct wined3d_allocator *allocator,
        struct wined3d_context *context, unsigned int memory_type, size_t chunk_size)
{
    struct wined3d_context_vk *context_vk = wined3d_context_vk(context);
    struct wined3d_allocator_chunk_vk *chunk_vk;

    if (!(chunk_vk = heap_alloc(sizeof(*chunk_vk))))
        return NULL;

    if (!wined3d_allocator_chunk_init(&chunk_vk->c, allocator))
    {
        heap_free(chunk_vk);
        return NULL;
    }

    if (!(chunk_vk->vk_memory = wined3d_context_vk_allocate_vram_chunk_memory(context_vk, memory_type, chunk_size)))
    {
        wined3d_allocator_chunk_cleanup(&chunk_vk->c);
        heap_free(chunk_vk);
        return NULL;
    }
    list_add_head(&allocator->pools[memory_type].chunks, &chunk_vk->c.entry);

    return &chunk_vk->c;
}

static void wined3d_allocator_vk_destroy_chunk(struct wined3d_allocator_chunk *chunk)
{
    struct wined3d_allocator_chunk_vk *chunk_vk = wined3d_allocator_chunk_vk(chunk);
    const struct wined3d_vk_info *vk_info;
    struct wined3d_device_vk *device_vk;

    TRACE("chunk %p.\n", chunk);

    device_vk = CONTAINING_RECORD(chunk_vk->c.allocator, struct wined3d_device_vk, allocator);
    vk_info = &device_vk->vk_info;

    if (chunk_vk->c.map_ptr)
        VK_CALL(vkUnmapMemory(device_vk->vk_device, chunk_vk->vk_memory));
    VK_CALL(vkFreeMemory(device_vk->vk_device, chunk_vk->vk_memory, NULL));
    TRACE("Freed memory 0x%s.\n", wine_dbgstr_longlong(chunk_vk->vk_memory));
    wined3d_allocator_chunk_cleanup(&chunk_vk->c);
    heap_free(chunk_vk);
}

static const struct wined3d_allocator_ops wined3d_allocator_vk_ops =
{
    .allocator_create_chunk = wined3d_allocator_vk_create_chunk,
    .allocator_destroy_chunk = wined3d_allocator_vk_destroy_chunk,
};

static const struct
{
    const char *name;
    unsigned int core_since_version;
}
vulkan_device_extensions[] =
{
    {VK_EXT_VERTEX_ATTRIBUTE_DIVISOR_EXTENSION_NAME,    ~0u},
    {VK_KHR_MAINTENANCE1_EXTENSION_NAME,                VK_API_VERSION_1_1},
    {VK_KHR_SHADER_DRAW_PARAMETERS_EXTENSION_NAME,      VK_API_VERSION_1_1},
    {VK_KHR_SWAPCHAIN_EXTENSION_NAME,                   ~0u},
};

static bool enable_vulkan_device_extensions(VkPhysicalDevice physical_device, uint32_t *extension_count,
        const char *enabled_extensions[], const struct wined3d_vk_info *vk_info)
{
    VkExtensionProperties *extensions = NULL;
    bool success = false, found;
    unsigned int i, j, count;
    VkResult vr;

    *extension_count = 0;

    if ((vr = VK_CALL(vkEnumerateDeviceExtensionProperties(physical_device, NULL, &count, NULL))) < 0)
    {
        ERR("Failed to enumerate device extensions, vr %s.\n", wined3d_debug_vkresult(vr));
        goto done;
    }
    if (!(extensions = heap_calloc(count, sizeof(*extensions))))
    {
        WARN("Out of memory.\n");
        goto done;
    }
    if ((vr = VK_CALL(vkEnumerateDeviceExtensionProperties(physical_device, NULL, &count, extensions))) < 0)
    {
        ERR("Failed to enumerate device extensions, vr %s.\n", wined3d_debug_vkresult(vr));
        goto done;
    }

    for (i = 0; i < ARRAY_SIZE(vulkan_device_extensions); ++i)
    {
        if (vulkan_device_extensions[i].core_since_version <= vk_info->api_version)
            continue;

        for (j = 0, found = false; j < count; ++j)
        {
            if (!strcmp(extensions[j].extensionName, vulkan_device_extensions[i].name))
            {
                found = true;
                break;
            }
        }

        if (!found)
        {
            WARN("Required extension '%s' is not available.\n", vulkan_device_extensions[i].name);
            goto done;
        }

        TRACE("Enabling instance extension '%s'.\n", vulkan_device_extensions[i].name);
        enabled_extensions[(*extension_count)++] = vulkan_device_extensions[i].name;
    }
    success = true;

done:
    heap_free(extensions);
    return success;
}

static HRESULT adapter_vk_create_device(struct wined3d *wined3d, const struct wined3d_adapter *adapter,
        enum wined3d_device_type device_type, HWND focus_window, unsigned int flags, BYTE surface_alignment,
        const enum wined3d_feature_level *levels, unsigned int level_count,
        struct wined3d_device_parent *device_parent, struct wined3d_device **device)
{
    const struct wined3d_adapter_vk *adapter_vk = wined3d_adapter_vk_const(adapter);
    const char *enabled_device_extensions[ARRAY_SIZE(vulkan_device_extensions)];
    VkPhysicalDeviceVertexAttributeDivisorFeaturesEXT *vertex_divisor_features;
    const struct wined3d_vk_info *vk_info = &adapter_vk->vk_info;
    struct wined3d_physical_device_info physical_device_info;
    static const float priorities[] = {1.0f};
    VkPhysicalDeviceFeatures2 *features2;
    struct wined3d_device_vk *device_vk;
    VkDevice vk_device = VK_NULL_HANDLE;
    VkDeviceQueueCreateInfo queue_info;
    VkPhysicalDevice physical_device;
    VkDeviceCreateInfo device_info;
    uint32_t queue_family_index;
    uint32_t timestamp_bits;
    VkResult vr;
    HRESULT hr;

    if (!(device_vk = heap_alloc_zero(sizeof(*device_vk))))
        return E_OUTOFMEMORY;

    if (FAILED(hr = wined3d_select_vulkan_queue_family(adapter_vk, &queue_family_index, &timestamp_bits)))
        goto fail;

    physical_device = adapter_vk->physical_device;

    memset(&physical_device_info, 0, sizeof(physical_device_info));

    vertex_divisor_features = &physical_device_info.vertex_divisor_features;
    vertex_divisor_features->sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_ATTRIBUTE_DIVISOR_FEATURES_EXT;

    features2 = &physical_device_info.features2;
    features2->sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features2->pNext = vertex_divisor_features;

    if (vk_info->vk_ops.vkGetPhysicalDeviceFeatures2)
        VK_CALL(vkGetPhysicalDeviceFeatures2(physical_device, features2));
    else
        VK_CALL(vkGetPhysicalDeviceFeatures(physical_device, &features2->features));

    if (!vertex_divisor_features->vertexAttributeInstanceRateDivisor)
    {
        WARN("Vertex attribute divisors not supported.\n");
        hr = E_FAIL;
        goto fail;
    }

    wined3d_disable_vulkan_features(&physical_device_info);

    queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_info.pNext = NULL;
    queue_info.flags = 0;
    queue_info.queueFamilyIndex = queue_family_index;
    queue_info.queueCount = ARRAY_SIZE(priorities);
    queue_info.pQueuePriorities = priorities;

    device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_info.pNext = features2->pNext;
    device_info.flags = 0;
    device_info.queueCreateInfoCount = 1;
    device_info.pQueueCreateInfos = &queue_info;
    device_info.enabledLayerCount = 0;
    device_info.ppEnabledLayerNames = NULL;
    device_info.ppEnabledExtensionNames = enabled_device_extensions;
    if (!enable_vulkan_device_extensions(physical_device,
            &device_info.enabledExtensionCount, enabled_device_extensions, vk_info))
    {
        hr = E_FAIL;
        goto fail;
    }
    device_info.pEnabledFeatures = &features2->features;

    if ((vr = VK_CALL(vkCreateDevice(physical_device, &device_info, NULL, &vk_device))) < 0)
    {
        WARN("Failed to create Vulkan device, vr %s.\n", wined3d_debug_vkresult(vr));
        vk_device = VK_NULL_HANDLE;
        hr = hresult_from_vk_result(vr);
        goto fail;
    }

    device_vk->vk_device = vk_device;
    VK_CALL(vkGetDeviceQueue(vk_device, queue_family_index, 0, &device_vk->vk_queue));
    device_vk->vk_queue_family_index = queue_family_index;
    device_vk->timestamp_bits = timestamp_bits;

    device_vk->vk_info = *vk_info;
#define LOAD_DEVICE_PFN(name) \
    if (!(device_vk->vk_info.vk_ops.name = (void *)VK_CALL(vkGetDeviceProcAddr(vk_device, #name)))) \
    { \
        WARN("Could not get device proc addr for '" #name "'.\n"); \
        hr = E_FAIL; \
        goto fail; \
    }
#define VK_DEVICE_PFN LOAD_DEVICE_PFN
    VK_DEVICE_FUNCS()
#undef VK_DEVICE_PFN

    if (!wined3d_allocator_init(&device_vk->allocator,
            adapter_vk->memory_properties.memoryTypeCount, &wined3d_allocator_vk_ops))
    {
        WARN("Failed to initialise allocator.\n");
        hr = E_FAIL;
        goto fail;
    }

    if (FAILED(hr = wined3d_device_init(&device_vk->d, wined3d, adapter->ordinal, device_type, focus_window,
            flags, surface_alignment, levels, level_count, vk_info->supported, device_parent)))
    {
        WARN("Failed to initialize device, hr %#x.\n", hr);
        wined3d_allocator_cleanup(&device_vk->allocator);
        goto fail;
    }

    *device = &device_vk->d;

    return WINED3D_OK;

fail:
    VK_CALL(vkDestroyDevice(vk_device, NULL));
    heap_free(device_vk);
    return hr;
}

static void adapter_vk_destroy_device(struct wined3d_device *device)
{
    struct wined3d_device_vk *device_vk = wined3d_device_vk(device);
    const struct wined3d_vk_info *vk_info = &device_vk->vk_info;

    wined3d_device_cleanup(&device_vk->d);
    wined3d_allocator_cleanup(&device_vk->allocator);
    VK_CALL(vkDestroyDevice(device_vk->vk_device, NULL));
    heap_free(device_vk);
}

struct wined3d_context *adapter_vk_acquire_context(struct wined3d_device *device,
        struct wined3d_texture *texture, unsigned int sub_resource_idx)
{
    TRACE("device %p, texture %p, sub_resource_idx %u.\n", device, texture, sub_resource_idx);

    wined3d_from_cs(device->cs);

    if (!device->context_count)
        return NULL;

    return &wined3d_device_vk(device)->context_vk.c;
}

void adapter_vk_release_context(struct wined3d_context *context)
{
    TRACE("context %p.\n", context);
}

static void adapter_vk_get_wined3d_caps(const struct wined3d_adapter *adapter, struct wined3d_caps *caps)
{
    const struct wined3d_adapter_vk *adapter_vk = wined3d_adapter_vk_const(adapter);
    const VkPhysicalDeviceLimits *limits = &adapter_vk->device_limits;
    BOOL sampler_anisotropy = limits->maxSamplerAnisotropy > 1.0f;

    caps->ddraw_caps.dds_caps |= WINEDDSCAPS_BACKBUFFER
            | WINEDDSCAPS_FLIP
            | WINEDDSCAPS_COMPLEX
            | WINEDDSCAPS_FRONTBUFFER
            | WINEDDSCAPS_3DDEVICE
            | WINEDDSCAPS_VIDEOMEMORY
            | WINEDDSCAPS_OWNDC
            | WINEDDSCAPS_LOCALVIDMEM
            | WINEDDSCAPS_NONLOCALVIDMEM;
    caps->ddraw_caps.caps |= WINEDDCAPS_3D;

    caps->Caps2 |= WINED3DCAPS2_CANGENMIPMAP;

    caps->PrimitiveMiscCaps |= WINED3DPMISCCAPS_BLENDOP
            | WINED3DPMISCCAPS_INDEPENDENTWRITEMASKS
            | WINED3DPMISCCAPS_MRTINDEPENDENTBITDEPTHS
            | WINED3DPMISCCAPS_POSTBLENDSRGBCONVERT
            | WINED3DPMISCCAPS_SEPARATEALPHABLEND;

    caps->RasterCaps |= WINED3DPRASTERCAPS_MIPMAPLODBIAS;

    if (sampler_anisotropy)
    {
        caps->RasterCaps |= WINED3DPRASTERCAPS_ANISOTROPY;

        caps->TextureFilterCaps |= WINED3DPTFILTERCAPS_MAGFANISOTROPIC
                | WINED3DPTFILTERCAPS_MINFANISOTROPIC;

        caps->MaxAnisotropy = limits->maxSamplerAnisotropy;
    }

    caps->SrcBlendCaps |= WINED3DPBLENDCAPS_BLENDFACTOR;
    caps->DestBlendCaps |= WINED3DPBLENDCAPS_BLENDFACTOR
            | WINED3DPBLENDCAPS_SRCALPHASAT;

    caps->TextureCaps |= WINED3DPTEXTURECAPS_VOLUMEMAP
            | WINED3DPTEXTURECAPS_MIPVOLUMEMAP
            | WINED3DPTEXTURECAPS_VOLUMEMAP_POW2;
    caps->VolumeTextureFilterCaps |= WINED3DPTFILTERCAPS_MAGFLINEAR
            | WINED3DPTFILTERCAPS_MAGFPOINT
            | WINED3DPTFILTERCAPS_MINFLINEAR
            | WINED3DPTFILTERCAPS_MINFPOINT
            | WINED3DPTFILTERCAPS_MIPFLINEAR
            | WINED3DPTFILTERCAPS_MIPFPOINT
            | WINED3DPTFILTERCAPS_LINEAR
            | WINED3DPTFILTERCAPS_LINEARMIPLINEAR
            | WINED3DPTFILTERCAPS_LINEARMIPNEAREST
            | WINED3DPTFILTERCAPS_MIPLINEAR
            | WINED3DPTFILTERCAPS_MIPNEAREST
            | WINED3DPTFILTERCAPS_NEAREST;
    caps->VolumeTextureAddressCaps |= WINED3DPTADDRESSCAPS_INDEPENDENTUV
            | WINED3DPTADDRESSCAPS_CLAMP
            | WINED3DPTADDRESSCAPS_WRAP;
    caps->VolumeTextureAddressCaps |= WINED3DPTADDRESSCAPS_BORDER
            | WINED3DPTADDRESSCAPS_MIRROR
            | WINED3DPTADDRESSCAPS_MIRRORONCE;

    caps->MaxVolumeExtent = limits->maxImageDimension3D;

    caps->TextureCaps |= WINED3DPTEXTURECAPS_CUBEMAP
            | WINED3DPTEXTURECAPS_MIPCUBEMAP
            | WINED3DPTEXTURECAPS_CUBEMAP_POW2;
    caps->CubeTextureFilterCaps |= WINED3DPTFILTERCAPS_MAGFLINEAR
            | WINED3DPTFILTERCAPS_MAGFPOINT
            | WINED3DPTFILTERCAPS_MINFLINEAR
            | WINED3DPTFILTERCAPS_MINFPOINT
            | WINED3DPTFILTERCAPS_MIPFLINEAR
            | WINED3DPTFILTERCAPS_MIPFPOINT
            | WINED3DPTFILTERCAPS_LINEAR
            | WINED3DPTFILTERCAPS_LINEARMIPLINEAR
            | WINED3DPTFILTERCAPS_LINEARMIPNEAREST
            | WINED3DPTFILTERCAPS_MIPLINEAR
            | WINED3DPTFILTERCAPS_MIPNEAREST
            | WINED3DPTFILTERCAPS_NEAREST;

    if (sampler_anisotropy)
    {
        caps->CubeTextureFilterCaps |= WINED3DPTFILTERCAPS_MAGFANISOTROPIC
                | WINED3DPTFILTERCAPS_MINFANISOTROPIC;
    }

    caps->TextureAddressCaps |= WINED3DPTADDRESSCAPS_BORDER
            | WINED3DPTADDRESSCAPS_MIRROR
            | WINED3DPTADDRESSCAPS_MIRRORONCE;

    caps->StencilCaps |= WINED3DSTENCILCAPS_DECR
            | WINED3DSTENCILCAPS_INCR
            | WINED3DSTENCILCAPS_TWOSIDED;

    caps->DeclTypes |= WINED3DDTCAPS_FLOAT16_2 | WINED3DDTCAPS_FLOAT16_4;

    caps->MaxPixelShader30InstructionSlots = WINED3DMAX30SHADERINSTRUCTIONS;
    caps->MaxVertexShader30InstructionSlots = WINED3DMAX30SHADERINSTRUCTIONS;
    caps->PS20Caps.temp_count = WINED3DPS20_MAX_NUMTEMPS;
    caps->VS20Caps.temp_count = WINED3DVS20_MAX_NUMTEMPS;
}

static BOOL adapter_vk_check_format(const struct wined3d_adapter *adapter,
        const struct wined3d_format *adapter_format, const struct wined3d_format *rt_format,
        const struct wined3d_format *ds_format)
{
    return TRUE;
}

static HRESULT adapter_vk_init_3d(struct wined3d_device *device)
{
    struct wined3d_context_vk *context_vk;
    struct wined3d_device_vk *device_vk;
    HRESULT hr;

    TRACE("device %p.\n", device);

    device_vk = wined3d_device_vk(device);
    context_vk = &device_vk->context_vk;
    if (FAILED(hr = wined3d_context_vk_init(context_vk, device->swapchains[0])))
    {
        WARN("Failed to initialise context.\n");
        return hr;
    }

    if (FAILED(hr = device->shader_backend->shader_alloc_private(device,
            device->adapter->vertex_pipe, device->adapter->fragment_pipe)))
    {
        ERR("Failed to allocate shader private data, hr %#x.\n", hr);
        wined3d_context_vk_cleanup(context_vk);
        return hr;
    }

    if (!device_context_add(device, &context_vk->c))
    {
        ERR("Failed to add the newly created context to the context list.\n");
        device->shader_backend->shader_free_private(device, NULL);
        wined3d_context_vk_cleanup(context_vk);
        return E_FAIL;
    }

    TRACE("Initialised context %p.\n", context_vk);

    if (!(device_vk->d.blitter = wined3d_cpu_blitter_create()))
    {
        ERR("Failed to create CPU blitter.\n");
        device_context_remove(device, &context_vk->c);
        device->shader_backend->shader_free_private(device, NULL);
        wined3d_context_vk_cleanup(context_vk);
        return E_FAIL;
    }
    wined3d_vk_blitter_create(&device_vk->d.blitter);

    wined3d_device_create_default_samplers(device, &context_vk->c);
    wined3d_device_vk_create_null_resources(device_vk, context_vk);
    wined3d_device_vk_create_null_views(device_vk, context_vk);

    return WINED3D_OK;
}

static void adapter_vk_uninit_3d_cs(void *object)
{
    struct wined3d_device_vk *device_vk = object;
    struct wined3d_context_vk *context_vk;
    struct wined3d_device *device;
    struct wined3d_shader *shader;

    TRACE("device_vk %p.\n", device_vk);

    context_vk = &device_vk->context_vk;
    device = &device_vk->d;

    LIST_FOR_EACH_ENTRY(shader, &device->shaders, struct wined3d_shader, shader_list_entry)
    {
        device->shader_backend->shader_destroy(shader);
    }

    device->blitter->ops->blitter_destroy(device->blitter, NULL);
    device->shader_backend->shader_free_private(device, &context_vk->c);
    wined3d_device_vk_destroy_null_views(device_vk, context_vk);
    wined3d_device_vk_destroy_null_resources(device_vk, context_vk);
    wined3d_device_destroy_default_samplers(device, &context_vk->c);
}

static void adapter_vk_uninit_3d(struct wined3d_device *device)
{
    struct wined3d_context_vk *context_vk;
    struct wined3d_device_vk *device_vk;

    TRACE("device %p.\n", device);

    device_vk = wined3d_device_vk(device);
    context_vk = &device_vk->context_vk;

    wined3d_cs_destroy_object(device->cs, adapter_vk_uninit_3d_cs, device_vk);
    wined3d_cs_finish(device->cs, WINED3D_CS_QUEUE_DEFAULT);

    device_context_remove(device, &context_vk->c);
    wined3d_context_vk_cleanup(context_vk);
}

static void *wined3d_bo_vk_map(struct wined3d_bo_vk *bo, struct wined3d_context_vk *context_vk)
{
    const struct wined3d_vk_info *vk_info;
    struct wined3d_device_vk *device_vk;
    struct wined3d_bo_slab_vk *slab;
    void *map_ptr;
    VkResult vr;

    if (bo->map_ptr)
        return bo->map_ptr;

    vk_info = context_vk->vk_info;
    device_vk = wined3d_device_vk(context_vk->c.device);

    if ((slab = bo->slab))
    {
        if (!(map_ptr = slab->map_ptr) && !(map_ptr = wined3d_bo_vk_map(&slab->bo, context_vk)))
        {
            ERR("Failed to map slab.\n");
            return NULL;
        }
        ++slab->map_count;
    }
    else if (bo->memory)
    {
        struct wined3d_allocator_chunk_vk *chunk_vk = wined3d_allocator_chunk_vk(bo->memory->chunk);

        if (!(map_ptr = wined3d_allocator_chunk_vk_map(chunk_vk, context_vk)))
        {
            ERR("Failed to map chunk.\n");
            return NULL;
        }
    }
    else if ((vr = VK_CALL(vkMapMemory(device_vk->vk_device, bo->vk_memory, 0, VK_WHOLE_SIZE, 0, &map_ptr))) < 0)
    {
        ERR("Failed to map memory, vr %s.\n", wined3d_debug_vkresult(vr));
        return NULL;
    }

    if (sizeof(map_ptr) >= sizeof(uint64_t))
        bo->map_ptr = map_ptr;

    return map_ptr;
}

static void wined3d_bo_vk_unmap(struct wined3d_bo_vk *bo, struct wined3d_context_vk *context_vk)
{
    const struct wined3d_vk_info *vk_info;
    struct wined3d_device_vk *device_vk;
    struct wined3d_bo_slab_vk *slab;

    if (bo->map_ptr)
        return;

    if ((slab = bo->slab))
    {
        if (--slab->map_count)
            return;

        wined3d_bo_vk_unmap(&slab->bo, context_vk);
        slab->map_ptr = NULL;
        return;
    }

    vk_info = context_vk->vk_info;
    device_vk = wined3d_device_vk(context_vk->c.device);

    if (bo->memory)
        wined3d_allocator_chunk_vk_unmap(wined3d_allocator_chunk_vk(bo->memory->chunk), context_vk);
    else
        VK_CALL(vkUnmapMemory(device_vk->vk_device, bo->vk_memory));
}

static VkAccessFlags vk_access_mask_from_buffer_usage(VkBufferUsageFlags usage)
{
    VkAccessFlags flags = 0;

    if (usage & VK_BUFFER_USAGE_VERTEX_BUFFER_BIT)
        flags |= VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
    if (usage & VK_BUFFER_USAGE_INDEX_BUFFER_BIT)
        flags |= VK_ACCESS_INDEX_READ_BIT;
    if (usage & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)
        flags |= VK_ACCESS_UNIFORM_READ_BIT;
    if (usage & VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT)
        flags |= VK_ACCESS_SHADER_READ_BIT;
    if (usage & VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT)
        flags |= VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    if (usage & VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT)
        flags |= VK_ACCESS_INDIRECT_COMMAND_READ_BIT;

    return flags;
}

static void *adapter_vk_map_bo_address(struct wined3d_context *context,
        const struct wined3d_bo_address *data, size_t size, uint32_t map_flags)
{
    struct wined3d_context_vk *context_vk = wined3d_context_vk(context);
    const struct wined3d_vk_info *vk_info;
    struct wined3d_bo_user_vk *bo_user_vk;
    struct wined3d_device_vk *device_vk;
    VkCommandBuffer vk_command_buffer;
    VkBufferMemoryBarrier vk_barrier;
    struct wined3d_bo_vk *bo, tmp;
    VkMappedMemoryRange range;
    void *map_ptr;

    if (!(bo = (struct wined3d_bo_vk *)data->buffer_object))
        return data->addr;

    vk_info = context_vk->vk_info;
    device_vk = wined3d_device_vk(context->device);

    if (map_flags & WINED3D_MAP_NOOVERWRITE)
        goto map;

    if ((map_flags & WINED3D_MAP_DISCARD) && bo->command_buffer_id > context_vk->completed_command_buffer_id)
    {
        if (wined3d_context_vk_create_bo(context_vk, bo->size, bo->usage, bo->memory_type, &tmp))
        {
            list_move_head(&tmp.users, &bo->users);
            wined3d_context_vk_destroy_bo(context_vk, bo);
            *bo = tmp;
            list_init(&bo->users);
            list_move_head(&bo->users, &tmp.users);
            LIST_FOR_EACH_ENTRY(bo_user_vk, &bo->users, struct wined3d_bo_user_vk, entry)
            {
                bo_user_vk->valid = false;
            }

            goto map;
        }

        ERR("Failed to create new buffer object.\n");
    }

    if (map_flags & WINED3D_MAP_READ)
    {
        if (!(vk_command_buffer = wined3d_context_vk_get_command_buffer(context_vk)))
        {
            ERR("Failed to get command buffer.\n");
            return NULL;
        }

        wined3d_context_vk_end_current_render_pass(context_vk);

        vk_barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        vk_barrier.pNext = NULL;
        vk_barrier.srcAccessMask = vk_access_mask_from_buffer_usage(bo->usage);
        vk_barrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
        vk_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        vk_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        vk_barrier.buffer = bo->vk_buffer;
        vk_barrier.offset = bo->buffer_offset + (uintptr_t)data->addr;
        vk_barrier.size = size;
        VK_CALL(vkCmdPipelineBarrier(vk_command_buffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                VK_PIPELINE_STAGE_HOST_BIT, 0, 0, NULL, 1, &vk_barrier, 0, NULL));

        if (!(bo->memory_type & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
        {
            range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
            range.pNext = NULL;
            range.memory = bo->vk_memory;
            range.offset = bo->memory_offset + (uintptr_t)data->addr;
            range.size = size;
            VK_CALL(vkInvalidateMappedMemoryRanges(device_vk->vk_device, 1, &range));
        }

        wined3d_context_vk_reference_bo(context_vk, bo);
    }

    if (bo->command_buffer_id == context_vk->current_command_buffer.id)
        wined3d_context_vk_submit_command_buffer(context_vk, 0, NULL, NULL, 0, NULL);
    wined3d_context_vk_wait_command_buffer(context_vk, bo->command_buffer_id);

map:
    if (!(map_ptr = wined3d_bo_vk_map(bo, context_vk)))
    {
        ERR("Failed to map bo.\n");
        return NULL;
    }

    return (uint8_t *)map_ptr + bo->memory_offset + (uintptr_t)data->addr;
}

static void adapter_vk_unmap_bo_address(struct wined3d_context *context,
        const struct wined3d_bo_address *data, unsigned int range_count, const struct wined3d_range *ranges)
{
    struct wined3d_context_vk *context_vk = wined3d_context_vk(context);
    const struct wined3d_vk_info *vk_info;
    struct wined3d_device_vk *device_vk;
    VkMappedMemoryRange range;
    struct wined3d_bo_vk *bo;
    unsigned int i;

    if (!(bo = (struct wined3d_bo_vk *)data->buffer_object))
        return;

    vk_info = context_vk->vk_info;
    device_vk = wined3d_device_vk(context->device);

    if (!(bo->memory_type & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
    {
        range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
        range.pNext = NULL;
        range.memory = bo->vk_memory;

        for (i = 0; i < range_count; ++i)
        {
            range.offset = bo->memory_offset + ranges[i].offset;
            range.size = ranges[i].size;
            VK_CALL(vkFlushMappedMemoryRanges(device_vk->vk_device, 1, &range));
        }
    }

    wined3d_bo_vk_unmap(bo, context_vk);
}

static void adapter_vk_copy_bo_address(struct wined3d_context *context,
        const struct wined3d_bo_address *dst, const struct wined3d_bo_address *src, size_t size)
{
    struct wined3d_context_vk *context_vk = wined3d_context_vk(context);
    const struct wined3d_vk_info *vk_info = context_vk->vk_info;
    struct wined3d_bo_vk staging_bo, *src_bo, *dst_bo;
    VkAccessFlags src_access_mask, dst_access_mask;
    VkBufferMemoryBarrier vk_barrier[2];
    struct wined3d_bo_address staging;
    VkCommandBuffer vk_command_buffer;
    struct wined3d_range range;
    void *dst_ptr, *src_ptr;
    VkBufferCopy region;

    src_bo = (struct wined3d_bo_vk *)src->buffer_object;
    dst_bo = (struct wined3d_bo_vk *)dst->buffer_object;

    if (src_bo && dst_bo)
    {
        if (!(vk_command_buffer = wined3d_context_vk_get_command_buffer(context_vk)))
        {
            ERR("Failed to get command buffer.\n");
            return;
        }

        wined3d_context_vk_end_current_render_pass(context_vk);

        src_access_mask = vk_access_mask_from_buffer_usage(src_bo->usage);
        dst_access_mask = vk_access_mask_from_buffer_usage(dst_bo->usage);

        region.srcOffset = src_bo->buffer_offset + (uintptr_t)src->addr;
        region.dstOffset = dst_bo->buffer_offset + (uintptr_t)dst->addr;
        region.size = size;

        vk_barrier[0].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        vk_barrier[0].pNext = NULL;
        vk_barrier[0].srcAccessMask = src_access_mask;
        vk_barrier[0].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        vk_barrier[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        vk_barrier[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        vk_barrier[0].buffer = src_bo->vk_buffer;
        vk_barrier[0].offset = region.srcOffset;
        vk_barrier[0].size = region.size;

        vk_barrier[1].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        vk_barrier[1].pNext = NULL;
        vk_barrier[1].srcAccessMask = dst_access_mask;
        vk_barrier[1].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        vk_barrier[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        vk_barrier[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        vk_barrier[1].buffer = dst_bo->vk_buffer;
        vk_barrier[1].offset = region.dstOffset;
        vk_barrier[1].size = region.size;

        VK_CALL(vkCmdPipelineBarrier(vk_command_buffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 2, vk_barrier, 0, NULL));

        VK_CALL(vkCmdCopyBuffer(vk_command_buffer, src_bo->vk_buffer, dst_bo->vk_buffer, 1, &region));

        vk_barrier[0].srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        vk_barrier[0].dstAccessMask = src_access_mask;

        vk_barrier[1].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        vk_barrier[1].dstAccessMask = dst_access_mask;

        VK_CALL(vkCmdPipelineBarrier(vk_command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, NULL, 2, vk_barrier, 0, NULL));

        wined3d_context_vk_reference_bo(context_vk, src_bo);
        wined3d_context_vk_reference_bo(context_vk, dst_bo);

        return;
    }

    if (src_bo && !(src_bo->memory_type & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT))
    {
        if (!(wined3d_context_vk_create_bo(context_vk, size, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, &staging_bo)))
        {
            ERR("Failed to create staging bo.\n");
            return;
        }

        staging.buffer_object = (uintptr_t)&staging_bo;
        staging.addr = NULL;
        adapter_vk_copy_bo_address(context, &staging, src, size);
        adapter_vk_copy_bo_address(context, dst, &staging, size);

        wined3d_context_vk_destroy_bo(context_vk, &staging_bo);

        return;
    }

    if (dst_bo && (dst_bo->command_buffer_id > context_vk->completed_command_buffer_id
            || !(dst_bo->memory_type & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)))
    {
        if (!(wined3d_context_vk_create_bo(context_vk, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, &staging_bo)))
        {
            ERR("Failed to create staging bo.\n");
            return;
        }

        staging.buffer_object = (uintptr_t)&staging_bo;
        staging.addr = NULL;
        adapter_vk_copy_bo_address(context, &staging, src, size);
        adapter_vk_copy_bo_address(context, dst, &staging, size);

        wined3d_context_vk_destroy_bo(context_vk, &staging_bo);

        return;
    }

    src_ptr = adapter_vk_map_bo_address(context, src, size, WINED3D_MAP_READ);
    dst_ptr = adapter_vk_map_bo_address(context, dst, size, WINED3D_MAP_WRITE);

    memcpy(dst_ptr, src_ptr, size);

    range.offset = 0;
    range.size = size;
    adapter_vk_unmap_bo_address(context, dst, 1, &range);
    adapter_vk_unmap_bo_address(context, src, 0, NULL);
}

static HRESULT adapter_vk_create_swapchain(struct wined3d_device *device, struct wined3d_swapchain_desc *desc,
        void *parent, const struct wined3d_parent_ops *parent_ops, struct wined3d_swapchain **swapchain)
{
    struct wined3d_swapchain_vk *swapchain_vk;
    HRESULT hr;

    TRACE("device %p, desc %p, parent %p, parent_ops %p, swapchain %p.\n",
            device, desc, parent, parent_ops, swapchain);

    if (!(swapchain_vk = heap_alloc_zero(sizeof(*swapchain_vk))))
        return E_OUTOFMEMORY;

    if (FAILED(hr = wined3d_swapchain_vk_init(swapchain_vk, device, desc, parent, parent_ops)))
    {
        WARN("Failed to initialise swapchain, hr %#x.\n", hr);
        heap_free(swapchain_vk);
        return hr;
    }

    TRACE("Created swapchain %p.\n", swapchain_vk);
    *swapchain = &swapchain_vk->s;

    return hr;
}

static void adapter_vk_destroy_swapchain(struct wined3d_swapchain *swapchain)
{
    struct wined3d_swapchain_vk *swapchain_vk = wined3d_swapchain_vk(swapchain);

    wined3d_swapchain_vk_cleanup(swapchain_vk);
    heap_free(swapchain_vk);
}

unsigned int wined3d_adapter_vk_get_memory_type_index(const struct wined3d_adapter_vk *adapter_vk,
        uint32_t memory_type_mask, VkMemoryPropertyFlags flags)
{
    const VkPhysicalDeviceMemoryProperties *memory_info = &adapter_vk->memory_properties;
    unsigned int i;

    for (i = 0; i < memory_info->memoryTypeCount; ++i)
    {
        if (!(memory_type_mask & (1u << i)))
            continue;
        if ((memory_info->memoryTypes[i].propertyFlags & flags) == flags)
            return i;
    }

    return ~0u;
}

static HRESULT adapter_vk_create_buffer(struct wined3d_device *device,
        const struct wined3d_buffer_desc *desc, const struct wined3d_sub_resource_data *data,
        void *parent, const struct wined3d_parent_ops *parent_ops, struct wined3d_buffer **buffer)
{
    struct wined3d_buffer_vk *buffer_vk;
    HRESULT hr;

    TRACE("device %p, desc %p, data %p, parent %p, parent_ops %p, buffer %p.\n",
            device, desc, data, parent, parent_ops, buffer);

    if (!(buffer_vk = heap_alloc_zero(sizeof(*buffer_vk))))
        return E_OUTOFMEMORY;

    if (FAILED(hr = wined3d_buffer_vk_init(buffer_vk, device, desc, data, parent, parent_ops)))
    {
        WARN("Failed to initialise buffer, hr %#x.\n", hr);
        heap_free(buffer_vk);
        return hr;
    }

    TRACE("Created buffer %p.\n", buffer_vk);
    *buffer = &buffer_vk->b;

    return hr;
}

static void adapter_vk_destroy_buffer(struct wined3d_buffer *buffer)
{
    struct wined3d_buffer_vk *buffer_vk = wined3d_buffer_vk(buffer);
    struct wined3d_device *device = buffer_vk->b.resource.device;
    unsigned int swapchain_count = device->swapchain_count;

    TRACE("buffer_vk %p.\n", buffer_vk);

    /* Take a reference to the device, in case releasing the buffer would
     * cause the device to be destroyed. However, swapchain resources don't
     * take a reference to the device, and we wouldn't want to increment the
     * refcount on a device that's in the process of being destroyed. */
    if (swapchain_count)
        wined3d_device_incref(device);
    wined3d_buffer_cleanup(&buffer_vk->b);
    wined3d_cs_destroy_object(device->cs, heap_free, buffer_vk);
    if (swapchain_count)
        wined3d_device_decref(device);
}

static HRESULT adapter_vk_create_texture(struct wined3d_device *device,
        const struct wined3d_resource_desc *desc, unsigned int layer_count, unsigned int level_count,
        uint32_t flags, void *parent, const struct wined3d_parent_ops *parent_ops, struct wined3d_texture **texture)
{
    struct wined3d_texture_vk *texture_vk;
    HRESULT hr;

    TRACE("device %p, desc %p, layer_count %u, level_count %u, flags %#x, parent %p, parent_ops %p, texture %p.\n",
            device, desc, layer_count, level_count, flags, parent, parent_ops, texture);

    if (!(texture_vk = wined3d_texture_allocate_object_memory(sizeof(*texture_vk), level_count, layer_count)))
        return E_OUTOFMEMORY;

    if (FAILED(hr = wined3d_texture_vk_init(texture_vk, device, desc,
            layer_count, level_count, flags, parent, parent_ops)))
    {
        WARN("Failed to initialise texture, hr %#x.\n", hr);
        heap_free(texture_vk);
        return hr;
    }

    TRACE("Created texture %p.\n", texture_vk);
    *texture = &texture_vk->t;

    return hr;
}

static void adapter_vk_destroy_texture(struct wined3d_texture *texture)
{
    struct wined3d_texture_vk *texture_vk = wined3d_texture_vk(texture);
    struct wined3d_device *device = texture_vk->t.resource.device;
    unsigned int swapchain_count = device->swapchain_count;

    TRACE("texture_vk %p.\n", texture_vk);

    /* Take a reference to the device, in case releasing the texture would
     * cause the device to be destroyed. However, swapchain resources don't
     * take a reference to the device, and we wouldn't want to increment the
     * refcount on a device that's in the process of being destroyed. */
    if (swapchain_count)
        wined3d_device_incref(device);

    wined3d_texture_sub_resources_destroyed(texture);
    texture->resource.parent_ops->wined3d_object_destroyed(texture->resource.parent);

    wined3d_texture_cleanup(&texture_vk->t);
    wined3d_cs_destroy_object(device->cs, heap_free, texture_vk);

    if (swapchain_count)
        wined3d_device_decref(device);
}

static HRESULT adapter_vk_create_rendertarget_view(const struct wined3d_view_desc *desc,
        struct wined3d_resource *resource, void *parent, const struct wined3d_parent_ops *parent_ops,
        struct wined3d_rendertarget_view **view)
{
    struct wined3d_rendertarget_view_vk *view_vk;
    HRESULT hr;

    TRACE("desc %s, resource %p, parent %p, parent_ops %p, view %p.\n",
            wined3d_debug_view_desc(desc, resource), resource, parent, parent_ops, view);

    if (!(view_vk = heap_alloc_zero(sizeof(*view_vk))))
        return E_OUTOFMEMORY;

    if (FAILED(hr = wined3d_rendertarget_view_vk_init(view_vk, desc, resource, parent, parent_ops)))
    {
        WARN("Failed to initialise view, hr %#x.\n", hr);
        heap_free(view_vk);
        return hr;
    }

    TRACE("Created render target view %p.\n", view_vk);
    *view = &view_vk->v;

    return hr;
}

struct wined3d_view_vk_destroy_ctx
{
    struct wined3d_device_vk *device_vk;
    VkBufferView *vk_buffer_view;
    VkImageView *vk_image_view;
    struct wined3d_bo_vk *vk_counter_bo;
    VkBufferView *vk_counter_view;
    uint64_t *command_buffer_id;
    void *object;
    struct wined3d_view_vk_destroy_ctx *free;
};

static void wined3d_view_vk_destroy_object(void *object)
{
    struct wined3d_view_vk_destroy_ctx *ctx = object;
    const struct wined3d_vk_info *vk_info;
    struct wined3d_device_vk *device_vk;
    struct wined3d_context *context;

    device_vk = ctx->device_vk;
    vk_info = &wined3d_adapter_vk(device_vk->d.adapter)->vk_info;
    context = context_acquire(&device_vk->d, NULL, 0);

    if (ctx->vk_buffer_view)
    {
        if (context)
        {
            wined3d_context_vk_destroy_buffer_view(wined3d_context_vk(context),
                    *ctx->vk_buffer_view, *ctx->command_buffer_id);
        }
        else
        {
            VK_CALL(vkDestroyBufferView(device_vk->vk_device, *ctx->vk_buffer_view, NULL));
            TRACE("Destroyed buffer view 0x%s.\n", wine_dbgstr_longlong(*ctx->vk_buffer_view));
        }
    }
    if (ctx->vk_image_view)
    {
        if (context)
        {
            wined3d_context_vk_destroy_image_view(wined3d_context_vk(context),
                    *ctx->vk_image_view, *ctx->command_buffer_id);
        }
        else
        {
            VK_CALL(vkDestroyImageView(device_vk->vk_device, *ctx->vk_image_view, NULL));
            TRACE("Destroyed image view 0x%s.\n", wine_dbgstr_longlong(*ctx->vk_image_view));
        }
    }
    if (ctx->vk_counter_bo && ctx->vk_counter_bo->vk_buffer)
        wined3d_context_vk_destroy_bo(wined3d_context_vk(context), ctx->vk_counter_bo);
    if (ctx->vk_counter_view)
    {
        if (context)
        {
            wined3d_context_vk_destroy_buffer_view(wined3d_context_vk(context),
                    *ctx->vk_counter_view, *ctx->command_buffer_id);
        }
        else
        {
            VK_CALL(vkDestroyBufferView(device_vk->vk_device, *ctx->vk_counter_view, NULL));
            TRACE("Destroyed counter buffer view 0x%s.\n", wine_dbgstr_longlong(*ctx->vk_counter_view));
        }
    }

    if (context)
        context_release(context);

    heap_free(ctx->object);
    heap_free(ctx->free);
}

static void wined3d_view_vk_destroy(struct wined3d_device *device, VkBufferView *vk_buffer_view,
        VkImageView *vk_image_view, struct wined3d_bo_vk *vk_counter_bo,
        VkBufferView *vk_counter_view, uint64_t *command_buffer_id, void *view_vk)
{
    struct wined3d_view_vk_destroy_ctx *ctx, c;

    if (!(ctx = heap_alloc(sizeof(*ctx))))
        ctx = &c;
    ctx->device_vk = wined3d_device_vk(device);
    ctx->vk_buffer_view = vk_buffer_view;
    ctx->vk_image_view = vk_image_view;
    ctx->vk_counter_bo = vk_counter_bo;
    ctx->vk_counter_view = vk_counter_view;
    ctx->command_buffer_id = command_buffer_id;
    ctx->object = view_vk;
    ctx->free = ctx != &c ? ctx : NULL;

    wined3d_cs_destroy_object(device->cs, wined3d_view_vk_destroy_object, ctx);
    if (ctx == &c)
        device->cs->ops->finish(device->cs, WINED3D_CS_QUEUE_DEFAULT);
}

static void adapter_vk_destroy_rendertarget_view(struct wined3d_rendertarget_view *view)
{
    struct wined3d_rendertarget_view_vk *view_vk = wined3d_rendertarget_view_vk(view);
    struct wined3d_device *device = view_vk->v.resource->device;
    unsigned int swapchain_count = device->swapchain_count;

    TRACE("view_vk %p.\n", view_vk);

    /* Take a reference to the device, in case releasing the view's resource
     * would cause the device to be destroyed. However, swapchain resources
     * don't take a reference to the device, and we wouldn't want to increment
     * the refcount on a device that's in the process of being destroyed. */
    if (swapchain_count)
        wined3d_device_incref(device);
    wined3d_rendertarget_view_cleanup(&view_vk->v);
    wined3d_view_vk_destroy(device, NULL, &view_vk->vk_image_view,
            NULL, NULL, &view_vk->command_buffer_id, view_vk);
    if (swapchain_count)
        wined3d_device_decref(device);
}

static HRESULT adapter_vk_create_shader_resource_view(const struct wined3d_view_desc *desc,
        struct wined3d_resource *resource, void *parent, const struct wined3d_parent_ops *parent_ops,
        struct wined3d_shader_resource_view **view)
{
    struct wined3d_shader_resource_view_vk *view_vk;
    HRESULT hr;

    TRACE("desc %s, resource %p, parent %p, parent_ops %p, view %p.\n",
            wined3d_debug_view_desc(desc, resource), resource, parent, parent_ops, view);

    if (!(view_vk = heap_alloc_zero(sizeof(*view_vk))))
        return E_OUTOFMEMORY;

    if (FAILED(hr = wined3d_shader_resource_view_vk_init(view_vk, desc, resource, parent, parent_ops)))
    {
        WARN("Failed to initialise view, hr %#x.\n", hr);
        heap_free(view_vk);
        return hr;
    }

    TRACE("Created shader resource view %p.\n", view_vk);
    *view = &view_vk->v;

    return hr;
}

static void adapter_vk_destroy_shader_resource_view(struct wined3d_shader_resource_view *view)
{
    struct wined3d_shader_resource_view_vk *srv_vk = wined3d_shader_resource_view_vk(view);
    struct wined3d_device *device = srv_vk->v.resource->device;
    unsigned int swapchain_count = device->swapchain_count;
    struct wined3d_view_vk *view_vk = &srv_vk->view_vk;
    VkBufferView *vk_buffer_view = NULL;
    VkImageView *vk_image_view = NULL;

    TRACE("srv_vk %p.\n", srv_vk);

    /* Take a reference to the device, in case releasing the view's resource
     * would cause the device to be destroyed. However, swapchain resources
     * don't take a reference to the device, and we wouldn't want to increment
     * the refcount on a device that's in the process of being destroyed. */
    if (swapchain_count)
        wined3d_device_incref(device);
    if (srv_vk->v.resource->type == WINED3D_RTYPE_BUFFER)
        vk_buffer_view = &view_vk->u.vk_buffer_view;
    else
        vk_image_view = &view_vk->u.vk_image_info.imageView;
    list_remove(&view_vk->bo_user.entry);
    wined3d_shader_resource_view_cleanup(&srv_vk->v);
    wined3d_view_vk_destroy(device, vk_buffer_view, vk_image_view,
            NULL, NULL, &view_vk->command_buffer_id, srv_vk);
    if (swapchain_count)
        wined3d_device_decref(device);
}

static HRESULT adapter_vk_create_unordered_access_view(const struct wined3d_view_desc *desc,
        struct wined3d_resource *resource, void *parent, const struct wined3d_parent_ops *parent_ops,
        struct wined3d_unordered_access_view **view)
{
    struct wined3d_unordered_access_view_vk *view_vk;
    HRESULT hr;

    TRACE("desc %s, resource %p, parent %p, parent_ops %p, view %p.\n",
            wined3d_debug_view_desc(desc, resource), resource, parent, parent_ops, view);

    if (!(view_vk = heap_alloc_zero(sizeof(*view_vk))))
        return E_OUTOFMEMORY;

    if (FAILED(hr = wined3d_unordered_access_view_vk_init(view_vk, desc, resource, parent, parent_ops)))
    {
        WARN("Failed to initialise view, hr %#x.\n", hr);
        heap_free(view_vk);
        return hr;
    }

    TRACE("Created unordered access view %p.\n", view_vk);
    *view = &view_vk->v;

    return hr;
}

static void adapter_vk_destroy_unordered_access_view(struct wined3d_unordered_access_view *view)
{
    struct wined3d_unordered_access_view_vk *uav_vk = wined3d_unordered_access_view_vk(view);
    struct wined3d_device *device = uav_vk->v.resource->device;
    unsigned int swapchain_count = device->swapchain_count;
    struct wined3d_view_vk *view_vk = &uav_vk->view_vk;
    VkBufferView *vk_buffer_view = NULL;
    VkImageView *vk_image_view = NULL;

    TRACE("uav_vk %p.\n", uav_vk);

    /* Take a reference to the device, in case releasing the view's resource
     * would cause the device to be destroyed. However, swapchain resources
     * don't take a reference to the device, and we wouldn't want to increment
     * the refcount on a device that's in the process of being destroyed. */
    if (swapchain_count)
        wined3d_device_incref(device);
    if (uav_vk->v.resource->type == WINED3D_RTYPE_BUFFER)
        vk_buffer_view = &view_vk->u.vk_buffer_view;
    else
        vk_image_view = &view_vk->u.vk_image_info.imageView;
    list_remove(&view_vk->bo_user.entry);
    wined3d_unordered_access_view_cleanup(&uav_vk->v);
    wined3d_view_vk_destroy(device, vk_buffer_view, vk_image_view, &uav_vk->counter_bo,
            &uav_vk->vk_counter_view, &view_vk->command_buffer_id, uav_vk);
    if (swapchain_count)
        wined3d_device_decref(device);
}

static HRESULT adapter_vk_create_sampler(struct wined3d_device *device, const struct wined3d_sampler_desc *desc,
        void *parent, const struct wined3d_parent_ops *parent_ops, struct wined3d_sampler **sampler)
{
    struct wined3d_sampler_vk *sampler_vk;

    TRACE("device %p, desc %p, parent %p, parent_ops %p, sampler %p.\n",
            device, desc, parent, parent_ops, sampler);

    if (!(sampler_vk = heap_alloc_zero(sizeof(*sampler_vk))))
        return E_OUTOFMEMORY;

    wined3d_sampler_vk_init(sampler_vk, device, desc, parent, parent_ops);

    TRACE("Created sampler %p.\n", sampler_vk);
    *sampler = &sampler_vk->s;

    return WINED3D_OK;
}

static void wined3d_sampler_vk_destroy_object(void *object)
{
    struct wined3d_sampler_vk *sampler_vk = object;
    struct wined3d_context_vk *context_vk;

    context_vk = wined3d_context_vk(context_acquire(sampler_vk->s.device, NULL, 0));

    wined3d_context_vk_destroy_sampler(context_vk, sampler_vk->vk_image_info.sampler, sampler_vk->command_buffer_id);
    heap_free(sampler_vk);

    context_release(&context_vk->c);
}

static void adapter_vk_destroy_sampler(struct wined3d_sampler *sampler)
{
    struct wined3d_sampler_vk *sampler_vk = wined3d_sampler_vk(sampler);

    TRACE("sampler_vk %p.\n", sampler_vk);

    wined3d_cs_destroy_object(sampler->device->cs, wined3d_sampler_vk_destroy_object, sampler_vk);
}

static HRESULT adapter_vk_create_query(struct wined3d_device *device, enum wined3d_query_type type,
        void *parent, const struct wined3d_parent_ops *parent_ops, struct wined3d_query **query)
{
    TRACE("device %p, type %#x, parent %p, parent_ops %p, query %p.\n",
            device, type, parent, parent_ops, query);

    return wined3d_query_vk_create(device, type, parent, parent_ops, query);
}

static void wined3d_query_vk_destroy_object(void *object)
{
    struct wined3d_query_vk *query_vk = object;

    query_vk->q.query_ops->query_destroy(&query_vk->q);
}

static void adapter_vk_destroy_query(struct wined3d_query *query)
{
    struct wined3d_query_vk *query_vk = wined3d_query_vk(query);

    TRACE("query_vk %p.\n", query_vk);

    wined3d_cs_destroy_object(query->device->cs, wined3d_query_vk_destroy_object, query_vk);
}

static void adapter_vk_flush_context(struct wined3d_context *context)
{
    struct wined3d_context_vk *context_vk = wined3d_context_vk(context);

    TRACE("context_vk %p.\n", context_vk);

    wined3d_context_vk_submit_command_buffer(context_vk, 0, NULL, NULL, 0, NULL);
}

static void adapter_vk_draw_primitive(struct wined3d_device *device,
        const struct wined3d_state *state, const struct wined3d_draw_parameters *parameters)
{
    struct wined3d_buffer_vk *indirect_vk = NULL;
    const struct wined3d_vk_info *vk_info;
    struct wined3d_context_vk *context_vk;
    VkCommandBuffer vk_command_buffer;
    uint32_t instance_count;

    TRACE("device %p, state %p, parameters %p.\n", device, state, parameters);

    context_vk = wined3d_context_vk(context_acquire(device, NULL, 0));
    vk_info = context_vk->vk_info;

    if (parameters->indirect)
        indirect_vk = wined3d_buffer_vk(parameters->u.indirect.buffer);

    if (!(vk_command_buffer = wined3d_context_vk_apply_draw_state(context_vk,
            state, indirect_vk, parameters->indexed)))
    {
        ERR("Failed to apply draw state.\n");
        context_release(&context_vk->c);
        return;
    }

    if (parameters->indirect)
    {
        struct wined3d_bo_vk *bo = &indirect_vk->bo;
        uint32_t stride, size;

        wined3d_context_vk_reference_bo(context_vk, bo);
        size = indirect_vk->b.resource.size - parameters->u.indirect.offset;

        if (parameters->indexed)
        {
            stride = sizeof(VkDrawIndexedIndirectCommand);
            VK_CALL(vkCmdDrawIndexedIndirect(vk_command_buffer, bo->vk_buffer,
                    bo->buffer_offset + parameters->u.indirect.offset, size / stride, stride));
        }
        else
        {
            stride = sizeof(VkDrawIndirectCommand);
            VK_CALL(vkCmdDrawIndirect(vk_command_buffer, bo->vk_buffer,
                    bo->buffer_offset + parameters->u.indirect.offset, size / stride, stride));
        }
    }
    else
    {
        instance_count = parameters->u.direct.instance_count;
        if (context_vk->c.instance_count)
            instance_count = context_vk->c.instance_count;
        if (!instance_count)
            instance_count = 1;

        if (parameters->indexed)
            VK_CALL(vkCmdDrawIndexed(vk_command_buffer, parameters->u.direct.index_count,
                    instance_count, parameters->u.direct.start_idx, parameters->u.direct.base_vertex_idx,
                    parameters->u.direct.start_instance));
        else
            VK_CALL(vkCmdDraw(vk_command_buffer, parameters->u.direct.index_count, instance_count,
                    parameters->u.direct.start_idx, parameters->u.direct.start_instance));
    }

    context_release(&context_vk->c);
}

static void adapter_vk_dispatch_compute(struct wined3d_device *device,
        const struct wined3d_state *state, const struct wined3d_dispatch_parameters *parameters)
{
    struct wined3d_buffer_vk *indirect_vk = NULL;
    const struct wined3d_vk_info *vk_info;
    struct wined3d_context_vk *context_vk;
    VkCommandBuffer vk_command_buffer;

    TRACE("device %p, state %p, parameters %p.\n", device, state, parameters);

    context_vk = wined3d_context_vk(context_acquire(device, NULL, 0));
    vk_info = context_vk->vk_info;

    if (parameters->indirect)
        indirect_vk = wined3d_buffer_vk(parameters->u.indirect.buffer);

    if (!(vk_command_buffer = wined3d_context_vk_apply_compute_state(context_vk, state, indirect_vk)))
    {
        ERR("Failed to apply compute state.\n");
        context_release(&context_vk->c);
        return;
    }

    if (parameters->indirect)
    {
        struct wined3d_bo_vk *bo = &indirect_vk->bo;

        wined3d_context_vk_reference_bo(context_vk, bo);
        VK_CALL(vkCmdDispatchIndirect(vk_command_buffer, bo->vk_buffer,
                bo->buffer_offset + parameters->u.indirect.offset));
    }
    else
    {
        const struct wined3d_direct_dispatch_parameters *direct = &parameters->u.direct;

        VK_CALL(vkCmdDispatch(vk_command_buffer, direct->group_count_x, direct->group_count_y, direct->group_count_z));
    }

    VK_CALL(vkCmdPipelineBarrier(vk_command_buffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, 0, 0, NULL, 0, NULL, 0, NULL));

    context_release(&context_vk->c);
}

void adapter_vk_clear_uav(struct wined3d_context *context,
        struct wined3d_unordered_access_view *view, const struct wined3d_uvec4 *clear_value)
{
    TRACE("context %p, view %p, clear_value %s.\n", context, view, debug_uvec4(clear_value));

    wined3d_unordered_access_view_vk_clear_uint(wined3d_unordered_access_view_vk(view),
            clear_value, wined3d_context_vk(context));
}

static const struct wined3d_adapter_ops wined3d_adapter_vk_ops =
{
    .adapter_destroy = adapter_vk_destroy,
    .adapter_create_device = adapter_vk_create_device,
    .adapter_destroy_device = adapter_vk_destroy_device,
    .adapter_acquire_context = adapter_vk_acquire_context,
    .adapter_release_context = adapter_vk_release_context,
    .adapter_get_wined3d_caps = adapter_vk_get_wined3d_caps,
    .adapter_check_format = adapter_vk_check_format,
    .adapter_init_3d = adapter_vk_init_3d,
    .adapter_uninit_3d = adapter_vk_uninit_3d,
    .adapter_map_bo_address = adapter_vk_map_bo_address,
    .adapter_unmap_bo_address = adapter_vk_unmap_bo_address,
    .adapter_copy_bo_address = adapter_vk_copy_bo_address,
    .adapter_create_swapchain = adapter_vk_create_swapchain,
    .adapter_destroy_swapchain = adapter_vk_destroy_swapchain,
    .adapter_create_buffer = adapter_vk_create_buffer,
    .adapter_destroy_buffer = adapter_vk_destroy_buffer,
    .adapter_create_texture = adapter_vk_create_texture,
    .adapter_destroy_texture = adapter_vk_destroy_texture,
    .adapter_create_rendertarget_view = adapter_vk_create_rendertarget_view,
    .adapter_destroy_rendertarget_view = adapter_vk_destroy_rendertarget_view,
    .adapter_create_shader_resource_view = adapter_vk_create_shader_resource_view,
    .adapter_destroy_shader_resource_view = adapter_vk_destroy_shader_resource_view,
    .adapter_create_unordered_access_view = adapter_vk_create_unordered_access_view,
    .adapter_destroy_unordered_access_view = adapter_vk_destroy_unordered_access_view,
    .adapter_create_sampler = adapter_vk_create_sampler,
    .adapter_destroy_sampler = adapter_vk_destroy_sampler,
    .adapter_create_query = adapter_vk_create_query,
    .adapter_destroy_query = adapter_vk_destroy_query,
    .adapter_flush_context = adapter_vk_flush_context,
    .adapter_draw_primitive = adapter_vk_draw_primitive,
    .adapter_dispatch_compute = adapter_vk_dispatch_compute,
    .adapter_clear_uav = adapter_vk_clear_uav,
};

static unsigned int wined3d_get_wine_vk_version(void)
{
    const char *ptr = PACKAGE_VERSION;
    int major, minor;

    major = atoi(ptr);

    while (isdigit(*ptr))
        ++ptr;
    if (*ptr == '.')
        ++ptr;

    minor = atoi(ptr);

    return VK_MAKE_VERSION(major, minor, 0);
}

static const struct
{
    const char *name;
    unsigned int core_since_version;
    BOOL required;
}
vulkan_instance_extensions[] =
{
    {VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME, VK_API_VERSION_1_1, FALSE},
    {VK_KHR_SURFACE_EXTENSION_NAME,                          ~0u,                TRUE},
    {VK_KHR_WIN32_SURFACE_EXTENSION_NAME,                    ~0u,                TRUE},
};

static BOOL enable_vulkan_instance_extensions(uint32_t *extension_count,
        const char *enabled_extensions[], const struct wined3d_vk_info *vk_info)
{
    PFN_vkEnumerateInstanceExtensionProperties pfn_vkEnumerateInstanceExtensionProperties;
    VkExtensionProperties *extensions = NULL;
    BOOL success = FALSE, found;
    unsigned int i, j, count;
    VkResult vr;

    *extension_count = 0;

    if (!(pfn_vkEnumerateInstanceExtensionProperties
            = (void *)VK_CALL(vkGetInstanceProcAddr(NULL, "vkEnumerateInstanceExtensionProperties"))))
    {
        WARN("Failed to get 'vkEnumerateInstanceExtensionProperties'.\n");
        goto done;
    }

    if ((vr = pfn_vkEnumerateInstanceExtensionProperties(NULL, &count, NULL)) < 0)
    {
        WARN("Failed to count instance extensions, vr %s.\n", wined3d_debug_vkresult(vr));
        goto done;
    }
    if (!(extensions = heap_calloc(count, sizeof(*extensions))))
    {
        WARN("Out of memory.\n");
        goto done;
    }
    if ((vr = pfn_vkEnumerateInstanceExtensionProperties(NULL, &count, extensions)) < 0)
    {
        WARN("Failed to enumerate extensions, vr %s.\n", wined3d_debug_vkresult(vr));
        goto done;
    }

    for (i = 0; i < ARRAY_SIZE(vulkan_instance_extensions); ++i)
    {
        if (vulkan_instance_extensions[i].core_since_version <= vk_info->api_version)
            continue;

        for (j = 0, found = FALSE; j < count; ++j)
        {
            if (!strcmp(extensions[j].extensionName, vulkan_instance_extensions[i].name))
            {
                found = TRUE;
                break;
            }
        }
        if (found)
        {
            TRACE("Enabling instance extension '%s'.\n", vulkan_instance_extensions[i].name);
            enabled_extensions[(*extension_count)++] = vulkan_instance_extensions[i].name;
        }
        else if (!found && vulkan_instance_extensions[i].required)
        {
            WARN("Required extension '%s' is not available.\n", vulkan_instance_extensions[i].name);
            goto done;
        }
    }
    success = TRUE;

done:
    heap_free(extensions);
    return success;
}

static BOOL wined3d_init_vulkan(struct wined3d_vk_info *vk_info)
{
    const char *enabled_instance_extensions[ARRAY_SIZE(vulkan_instance_extensions)];
    PFN_vkEnumerateInstanceVersion pfn_vkEnumerateInstanceVersion;
    struct vulkan_ops *vk_ops = &vk_info->vk_ops;
    VkInstance instance = VK_NULL_HANDLE;
    VkInstanceCreateInfo instance_info;
    VkApplicationInfo app_info;
    uint32_t api_version = 0;
    char app_name[MAX_PATH];
    VkResult vr;

    if (!wined3d_load_vulkan(vk_info))
        return FALSE;

    if (!(vk_ops->vkCreateInstance = (void *)VK_CALL(vkGetInstanceProcAddr(NULL, "vkCreateInstance"))))
    {
        ERR("Failed to get 'vkCreateInstance'.\n");
        goto fail;
    }

    vk_info->api_version = VK_API_VERSION_1_0;
    if ((pfn_vkEnumerateInstanceVersion = (void *)VK_CALL(vkGetInstanceProcAddr(NULL, "vkEnumerateInstanceVersion")))
            && pfn_vkEnumerateInstanceVersion(&api_version) == VK_SUCCESS)
    {
        TRACE("Vulkan instance API version %s.\n", debug_vk_version(api_version));

        if (api_version >= VK_API_VERSION_1_1)
            vk_info->api_version = VK_API_VERSION_1_1;
    }

    memset(&app_info, 0, sizeof(app_info));
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    if (wined3d_get_app_name(app_name, ARRAY_SIZE(app_name)))
        app_info.pApplicationName = app_name;
    app_info.pEngineName = "Damavand";
    app_info.engineVersion = wined3d_get_wine_vk_version();
    app_info.apiVersion = vk_info->api_version;

    memset(&instance_info, 0, sizeof(instance_info));
    instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_info.pApplicationInfo = &app_info;
    instance_info.ppEnabledExtensionNames = enabled_instance_extensions;
    if (!enable_vulkan_instance_extensions(&instance_info.enabledExtensionCount, enabled_instance_extensions, vk_info))
        goto fail;

    memset(vk_info->supported, 0, sizeof(vk_info->supported));
    vk_info->supported[WINED3D_VK_EXT_NONE] = TRUE;

    if ((vr = VK_CALL(vkCreateInstance(&instance_info, NULL, &instance))) < 0)
    {
        WARN("Failed to create Vulkan instance, vr %s.\n", wined3d_debug_vkresult(vr));
        goto fail;
    }

    TRACE("Created Vulkan instance %p.\n", instance);

#define LOAD_INSTANCE_PFN(name) \
    if (!(vk_ops->name = (void *)VK_CALL(vkGetInstanceProcAddr(instance, #name)))) \
    { \
        WARN("Could not get instance proc addr for '" #name "'.\n"); \
        goto fail; \
    }
#define LOAD_INSTANCE_OPT_PFN(name) \
    vk_ops->name = (void *)VK_CALL(vkGetInstanceProcAddr(instance, #name));
#define VK_INSTANCE_PFN     LOAD_INSTANCE_PFN
#define VK_INSTANCE_EXT_PFN LOAD_INSTANCE_OPT_PFN
#define VK_DEVICE_PFN       LOAD_INSTANCE_PFN
    VK_INSTANCE_FUNCS()
    VK_DEVICE_FUNCS()
#undef VK_INSTANCE_PFN
#undef VK_INSTANCE_EXT_PFN
#undef VK_DEVICE_PFN

#define MAP_INSTANCE_FUNCTION(core_pfn, ext_pfn) \
    if (!vk_ops->core_pfn) \
        vk_ops->core_pfn = (void *)VK_CALL(vkGetInstanceProcAddr(instance, #ext_pfn));
    MAP_INSTANCE_FUNCTION(vkGetPhysicalDeviceProperties2, vkGetPhysicalDeviceProperties2KHR)
    MAP_INSTANCE_FUNCTION(vkGetPhysicalDeviceFeatures2, vkGetPhysicalDeviceFeatures2KHR)
#undef MAP_INSTANCE_FUNCTION

    vk_info->instance = instance;

    return TRUE;

fail:
    if (vk_ops->vkDestroyInstance)
        VK_CALL(vkDestroyInstance(instance, NULL));
    wined3d_unload_vulkan(vk_info);
    return FALSE;
}

static VkPhysicalDevice get_vulkan_physical_device(struct wined3d_vk_info *vk_info)
{
    VkPhysicalDevice physical_devices[1];
    uint32_t count;
    VkResult vr;

    if ((vr = VK_CALL(vkEnumeratePhysicalDevices(vk_info->instance, &count, NULL))) < 0)
    {
        WARN("Failed to enumerate physical devices, vr %s.\n", wined3d_debug_vkresult(vr));
        return VK_NULL_HANDLE;
    }
    if (!count)
    {
        WARN("No physical device.\n");
        return VK_NULL_HANDLE;
    }
    if (count > 1)
    {
        /* TODO: Create wined3d_adapter for each device. */
        FIXME("Multiple physical devices available.\n");
        count = 1;
    }

    if ((vr = VK_CALL(vkEnumeratePhysicalDevices(vk_info->instance, &count, physical_devices))) < 0)
    {
        WARN("Failed to get physical devices, vr %s.\n", wined3d_debug_vkresult(vr));
        return VK_NULL_HANDLE;
    }

    return physical_devices[0];
}

static enum wined3d_display_driver guess_display_driver(enum wined3d_pci_vendor vendor)
{
    switch (vendor)
    {
        case HW_VENDOR_AMD:    return DRIVER_AMD_RX;
        case HW_VENDOR_INTEL:  return DRIVER_INTEL_HD4000;
        case HW_VENDOR_NVIDIA: return DRIVER_NVIDIA_GEFORCE8;
        default:               return DRIVER_WINE;
    }
}

static bool adapter_vk_init_driver_info(struct wined3d_adapter_vk *adapter_vk,
        const VkPhysicalDeviceProperties *properties)
{
    const VkPhysicalDeviceMemoryProperties *memory_properties = &adapter_vk->memory_properties;
    const struct wined3d_gpu_description *gpu_description;
    struct wined3d_gpu_description description;
    UINT64 vram_bytes, sysmem_bytes;
    const VkMemoryHeap *heap;
    unsigned int i;

    TRACE("Device name: %s.\n", debugstr_a(properties->deviceName));
    TRACE("Vendor ID: 0x%04x, Device ID: 0x%04x.\n", properties->vendorID, properties->deviceID);
    TRACE("Driver version: %#x.\n", properties->driverVersion);
    TRACE("API version: %s.\n", debug_vk_version(properties->apiVersion));

    for (i = 0, vram_bytes = 0, sysmem_bytes = 0; i < memory_properties->memoryHeapCount; ++i)
    {
        heap = &memory_properties->memoryHeaps[i];
        TRACE("Memory heap [%u]: flags %#x, size 0x%s.\n",
                i, heap->flags, wine_dbgstr_longlong(heap->size));
        if (heap->flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
            vram_bytes += heap->size;
        else
            sysmem_bytes += heap->size;
    }
    TRACE("Total device memory: 0x%s.\n", wine_dbgstr_longlong(vram_bytes));
    TRACE("Total shared system memory: 0x%s.\n", wine_dbgstr_longlong(sysmem_bytes));

    if (!(gpu_description = wined3d_get_user_override_gpu_description(properties->vendorID, properties->deviceID)))
        gpu_description = wined3d_get_gpu_description(properties->vendorID, properties->deviceID);

    if (!gpu_description)
    {
        FIXME("Failed to retrieve GPU description for device %s %04x:%04x.\n",
                debugstr_a(properties->deviceName), properties->vendorID, properties->deviceID);

        description.vendor = properties->vendorID;
        description.device = properties->deviceID;
        description.description = properties->deviceName;
        description.driver = guess_display_driver(properties->vendorID);
        description.vidmem = vram_bytes;

        gpu_description = &description;
    }

    return wined3d_driver_info_init(&adapter_vk->a.driver_info, gpu_description,
            adapter_vk->a.d3d_info.feature_level, vram_bytes, sysmem_bytes);
}

static enum wined3d_feature_level feature_level_from_caps(const struct shader_caps *shader_caps)
{
    unsigned int shader_model;

    shader_model = min(shader_caps->vs_version, shader_caps->ps_version);
    shader_model = min(shader_model, max(shader_caps->gs_version, 3));
    shader_model = min(shader_model, max(shader_caps->hs_version, 4));
    shader_model = min(shader_model, max(shader_caps->ds_version, 4));

    if (shader_model >= 5)
        return WINED3D_FEATURE_LEVEL_11_1;

    if (shader_model >= 4)
        return WINED3D_FEATURE_LEVEL_10_1;

    return WINED3D_FEATURE_LEVEL_NONE;
}

static void wined3d_adapter_vk_init_d3d_info(struct wined3d_adapter_vk *adapter_vk, uint32_t wined3d_creation_flags)
{
    struct wined3d_d3d_info *d3d_info = &adapter_vk->a.d3d_info;
    struct wined3d_vertex_caps vertex_caps;
    struct fragment_caps fragment_caps;
    struct shader_caps shader_caps;

    adapter_vk->a.shader_backend->shader_get_caps(&adapter_vk->a, &shader_caps);
    adapter_vk->a.vertex_pipe->vp_get_caps(&adapter_vk->a, &vertex_caps);
    adapter_vk->a.fragment_pipe->get_caps(&adapter_vk->a, &fragment_caps);

    d3d_info->limits.vs_version = shader_caps.vs_version;
    d3d_info->limits.hs_version = shader_caps.hs_version;
    d3d_info->limits.ds_version = shader_caps.ds_version;
    d3d_info->limits.gs_version = shader_caps.gs_version;
    d3d_info->limits.ps_version = shader_caps.ps_version;
    d3d_info->limits.cs_version = shader_caps.cs_version;
    d3d_info->limits.vs_uniform_count = shader_caps.vs_uniform_count;
    d3d_info->limits.ps_uniform_count = shader_caps.ps_uniform_count;
    d3d_info->limits.varying_count = shader_caps.varying_count;
    d3d_info->limits.ffp_textures = fragment_caps.MaxSimultaneousTextures;
    d3d_info->limits.ffp_blend_stages = fragment_caps.MaxTextureBlendStages;
    d3d_info->limits.ffp_vertex_blend_matrices = vertex_caps.max_vertex_blend_matrices;
    d3d_info->limits.active_light_count = vertex_caps.max_active_lights;

    d3d_info->limits.max_rt_count = WINED3D_MAX_RENDER_TARGETS;
    d3d_info->limits.max_clip_distances = WINED3D_MAX_CLIP_DISTANCES;
    d3d_info->limits.texture_size = adapter_vk->device_limits.maxImageDimension2D;
    d3d_info->limits.pointsize_max = adapter_vk->device_limits.pointSizeRange[1];

    d3d_info->wined3d_creation_flags = wined3d_creation_flags;

    d3d_info->xyzrhw = vertex_caps.xyzrhw;
    d3d_info->emulated_flatshading = vertex_caps.emulated_flatshading;
    d3d_info->ffp_generic_attributes = vertex_caps.ffp_generic_attributes;
    d3d_info->ffp_alpha_test = false;
    d3d_info->vs_clipping = !!(shader_caps.wined3d_caps & WINED3D_SHADER_CAP_VS_CLIPPING);
    d3d_info->shader_color_key = !!(fragment_caps.wined3d_caps & WINED3D_FRAGMENT_CAP_COLOR_KEY);
    d3d_info->shader_double_precision = !!(shader_caps.wined3d_caps & WINED3D_SHADER_CAP_DOUBLE_PRECISION);
    d3d_info->shader_output_interpolation = !!(shader_caps.wined3d_caps & WINED3D_SHADER_CAP_OUTPUT_INTERPOLATION);
    d3d_info->viewport_array_index_any_shader = false; /* VK_EXT_shader_viewport_index_layer */
    d3d_info->texture_npot = true;
    d3d_info->texture_npot_conditional = true;
    d3d_info->draw_base_vertex_offset = true;
    d3d_info->vertex_bgra = true;
    d3d_info->texture_swizzle = true;
    d3d_info->srgb_read_control = false;
    d3d_info->srgb_write_control = false;
    d3d_info->clip_control = true;
    d3d_info->full_ffp_varyings = !!(shader_caps.wined3d_caps & WINED3D_SHADER_CAP_FULL_FFP_VARYINGS);
    d3d_info->scaled_resolve = false;
    d3d_info->feature_level = feature_level_from_caps(&shader_caps);

    d3d_info->multisample_draw_location = WINED3D_LOCATION_TEXTURE_RGB;
}

static BOOL wined3d_adapter_vk_init(struct wined3d_adapter_vk *adapter_vk,
        unsigned int ordinal, unsigned int wined3d_creation_flags)
{
    struct wined3d_vk_info *vk_info = &adapter_vk->vk_info;
    struct wined3d_adapter *adapter = &adapter_vk->a;
    VkPhysicalDeviceIDProperties id_properties;
    VkPhysicalDeviceProperties2 properties2;
    LUID *luid = NULL;

    TRACE("adapter_vk %p, ordinal %u, wined3d_creation_flags %#x.\n",
            adapter_vk, ordinal, wined3d_creation_flags);

    if (!wined3d_init_vulkan(vk_info))
    {
        WARN("Failed to initialize Vulkan.\n");
        return FALSE;
    }

    if (!(adapter_vk->physical_device = get_vulkan_physical_device(vk_info)))
        goto fail_vulkan;

    memset(&id_properties, 0, sizeof(id_properties));
    id_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES;
    properties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    properties2.pNext = &id_properties;

    if (vk_info->vk_ops.vkGetPhysicalDeviceProperties2)
        VK_CALL(vkGetPhysicalDeviceProperties2(adapter_vk->physical_device, &properties2));
    else
        VK_CALL(vkGetPhysicalDeviceProperties(adapter_vk->physical_device, &properties2.properties));
    adapter_vk->device_limits = properties2.properties.limits;

    VK_CALL(vkGetPhysicalDeviceMemoryProperties(adapter_vk->physical_device, &adapter_vk->memory_properties));

    if (id_properties.deviceLUIDValid)
        luid = (LUID *)id_properties.deviceLUID;

    if (!wined3d_adapter_init(adapter, ordinal, luid, &wined3d_adapter_vk_ops))
        goto fail_vulkan;

    adapter->vertex_pipe = wined3d_spirv_vertex_pipe_init_vk();
    adapter->fragment_pipe = wined3d_spirv_fragment_pipe_init_vk();
    adapter->misc_state_template = misc_state_template_vk;
    adapter->shader_backend = wined3d_spirv_shader_backend_init_vk();

    wined3d_adapter_vk_init_d3d_info(adapter_vk, wined3d_creation_flags);

    if (!adapter_vk_init_driver_info(adapter_vk, &properties2.properties))
        goto fail;
    TRACE("Reporting (fake) driver version 0x%08x-0x%08x.\n",
            adapter_vk->a.driver_info.version_high, adapter_vk->a.driver_info.version_low);
    adapter->vram_bytes_used = 0;
    TRACE("Emulating 0x%s bytes of video ram.\n", wine_dbgstr_longlong(adapter->driver_info.vram_bytes));

    memcpy(&adapter->driver_uuid, id_properties.driverUUID, sizeof(adapter->driver_uuid));
    memcpy(&adapter->device_uuid, id_properties.deviceUUID, sizeof(adapter->device_uuid));

    if (!wined3d_adapter_vk_init_format_info(adapter_vk, vk_info))
        goto fail;

    return TRUE;

fail:
    wined3d_adapter_cleanup(adapter);
fail_vulkan:
    VK_CALL(vkDestroyInstance(vk_info->instance, NULL));
    wined3d_unload_vulkan(vk_info);
    return FALSE;
}

struct wined3d_adapter *wined3d_adapter_vk_create(unsigned int ordinal,
        unsigned int wined3d_creation_flags)
{
    struct wined3d_adapter_vk *adapter_vk;

    if (!(adapter_vk = heap_alloc_zero(sizeof(*adapter_vk))))
        return NULL;

    if (!wined3d_adapter_vk_init(adapter_vk, ordinal, wined3d_creation_flags))
    {
        heap_free(adapter_vk);
        return NULL;
    }

    TRACE("Created adapter %p.\n", adapter_vk);

    return &adapter_vk->a;
}
