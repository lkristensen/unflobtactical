/*
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "renderQueue.h"
#include "model.h"    
#include "texture.h"
#include "gpustatemanager.h"
#include "../grinliz/glperformance.h"

using namespace grinliz;

RenderQueue::RenderQueue()
{
	nState = 0;
	nItem = 0;
}


RenderQueue::~RenderQueue()
{
}


RenderQueue::State* RenderQueue::FindState( const State& state )
{
	int low = 0;
	int high = nState - 1;
	int answer = -1;
	int mid = 0;
	int insert = 0;		// Where to put the new state. 

	while (low <= high) {
		mid = low + (high - low) / 2;
		int compare = Compare( statePool[mid], state );

		if (compare > 0) {
			high = mid - 1;
		}
		else if ( compare < 0) {
			insert = mid;	// since we know the mid is less than the state we want to add,
							// we can move insert up at least to that index.
			low = mid + 1;
		}
		else {
           answer = mid;
		   break;
		}
	}
	if ( answer >= 0 ) {
		return &statePool[answer];
	}
	if ( nState == MAX_STATE ) {
		return 0;
	}

	if ( nState == 0 ) {
		insert = 0;
	}
	else {
		while (    insert < nState
			    && Compare( statePool[insert], state ) < 0 ) 
		{
			++insert;
		}
	}

	// move up
	for( int i=nState; i>insert; --i ) {
		statePool[i] = statePool[i-1];
	}
	// and insert
	statePool[insert] = state;
	statePool[insert].root = 0;
	nState++;

#ifdef DEBUG
	for( int i=0; i<nState-1; ++i ) {
		//GLOUTPUT(( " %d:%d:%x", statePool[i].state.flags, statePool[i].state.textureID, statePool[i].state.atom ));
		GLASSERT( Compare( statePool[i], statePool[i+1] ) < 0 );
	}
	//GLOUTPUT(( "\n" ));
#endif

	return &statePool[insert];
}


void RenderQueue::Add( Model* model, const ModelAtom* atom, GPUShader* shader )
{
	if ( nItem == MAX_ITEMS ) {
		GLASSERT( 0 );
		return;
	}

	State s0 = { shader, atom->texture, 0 };

	State* state = FindState( s0 );
	if ( !state ) {
		GLASSERT( 0 );
		return;
	}

	Item* item = &itemPool[nItem++];
	item->model = model;
	item->atom = atom;

	item->next  = state->root;
	state->root = item;
}


void RenderQueue::Submit( GPUShader* shader, int mode, int required, int excluded, float bbRotation )
{
	GRINLIZ_PERFTRACK	

	for( int i=0; i<nState; ++i ) {
		for( Item* item = statePool[i].root; item; item=item->next ) 
		{
			Model* model = item->model;
			int modelFlags = model->Flags();

			if (    ( (required & modelFlags) == required)
				 && ( (excluded & modelFlags) == 0 ) )
			{
				if ( model->IsBillboard() )
					model->SetRotation( bbRotation );

				if ( !shader ) {
					GLASSERT( statePool[i].texture == item->atom->texture );
					statePool[i].shader->SetTexture0( statePool[i].texture );
				}

				const Model* model = item->model;
				GLASSERT( model );

				if ( mode & MODE_PLANAR_SHADOW ) {
					GLASSERT( shader );
					item->atom->BindPlanarShadow( shader );

					// Push the xform matrix to the texture and the model view.
					shader->PushMatrix( GPUShader::TEXTURE_MATRIX );
					shader->MultMatrix( GPUShader::TEXTURE_MATRIX, model->XForm() );

					shader->PushMatrix( GPUShader::MODELVIEW_MATRIX );
					shader->MultMatrix( GPUShader::MODELVIEW_MATRIX, model->XForm() );

					shader->Draw( item->atom->nIndex, item->atom->index );

					// Unravel all that.
					shader->PopMatrix( GPUShader::TEXTURE_MATRIX );
					shader->PopMatrix( GPUShader::MODELVIEW_MATRIX );
				}
				else {
					item->atom->Bind( shader ? shader : statePool[i].shader );

					GPUShader* s = statePool[i].shader;
					s->PushMatrix( GPUShader::MODELVIEW_MATRIX );
					s->MultMatrix( GPUShader::MODELVIEW_MATRIX, model->XForm() );
					if ( model->HasTextureXForm() ) {
						s->PushMatrix( GPUShader::TEXTURE_MATRIX );
						s->MultMatrix( GPUShader::TEXTURE_MATRIX, model->TextureXForm() );
					}
					
					s->Draw( item->atom->nIndex, item->atom->index );

					s->PopMatrix( GPUShader::MODELVIEW_MATRIX );
					if ( model->HasTextureXForm() ) {
						s->PopMatrix( GPUShader::TEXTURE_MATRIX );
					}
				}
			}
		}
	}
}
