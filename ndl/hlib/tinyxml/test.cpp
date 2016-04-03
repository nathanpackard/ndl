#include <iostream>
#include <sstream>
#include "tinyxml.h"

//~ void dump_to_stdout( TiXmlNode * pParent, unsigned int indent = 0 ){
    //~ if ( !pParent ) return;
    //~ TiXmlText *pText;
    //~ int t = pParent->Type();
    //~ //printf( "%s", getIndent( indent));
    //~ switch ( t ){
    //~ case TiXmlNode::DOCUMENT:
        //~ printf( "Document" );
        //~ break;
    //~ case TiXmlNode::ELEMENT:
        //~ printf( "Element \"%s\"", pParent->Value() );
        //~ break;
    //~ case TiXmlNode::COMMENT:
        //~ printf( "Comment: \"%s\"", pParent->Value());
        //~ break;
    //~ case TiXmlNode::UNKNOWN:
        //~ printf( "Unknown" );
        //~ break;
    //~ case TiXmlNode::TEXT:
        //~ pText = pParent->ToText();
        //~ printf( "Text: [%s]", pText->Value() );
        //~ break;
    //~ case TiXmlNode::DECLARATION:
        //~ printf( "Declaration" );
        //~ break;
    //~ default:
        //~ break;
    //~ }
    //~ printf( "\n" );
    //~ TiXmlNode * pChild;
    //~ for ( pChild = pParent->FirstChild(); pChild != 0; pChild = pChild->NextSibling()){
        //~ dump_to_stdout( pChild, indent+2 );
    //~ }
//~ }

int numviews(TiXmlNode* parent){
    int c=0;
    for (TiXmlNode* pChild = parent->FirstChild(); pChild != 0; pChild = pChild->NextSibling()){
        if (pChild->Type() == TiXmlNode::ELEMENT && strcmp(pChild->Value(),"View")==0){
            TiXmlElement* pElement = pChild->ToElement();
            printf( "View: viewerx=\"%s\", viewery=\"%s\"\n", pElement->Attribute("viewerx"), pElement->Attribute("viewery") );
            c++;
        }
    }
    return c;
}

int numimages(TiXmlNode* parent){
    int nimages=0;
    int nviews=0;
    for (TiXmlNode* pChild = parent->FirstChild(); pChild != 0; pChild = pChild->NextSibling()){
        if (pChild->Type() == TiXmlNode::ELEMENT && strcmp(pChild->Value(),"Image")==0){
            TiXmlElement* pElement = pChild->ToElement();
            printf( "Image: \"%s\"\n", pElement->Attribute("name") );
            nviews+=numviews(pChild);
            nimages++;
        }
    }
    printf("nimages: %d, nviews: %d\n",nimages,nviews);
    return nimages;
}

int numviewers(TiXmlNode* parent){
    int c=0;
    for (TiXmlNode* pChild = parent->FirstChild(); pChild != 0; pChild = pChild->NextSibling()){
        if (pChild->Type() == TiXmlNode::ELEMENT && strcmp(pChild->Value(),"Viewer")==0){
            TiXmlElement* pElement = pChild->ToElement();
            printf( "Viewer: \"%s\"\n", pElement->Attribute("type") );
            numimages(pChild);
            c++;
        }
    }
    return c;
}


int main(){
    TiXmlDocument doc( "ndlviewers.xml" );
    bool loadOkay = doc.LoadFile();
    if ( !loadOkay ){
        printf( "Could not load test file 'demotest.xml'. Error='%s'. Exiting.\n", doc.ErrorDesc() );
        exit(1);
    }
    numviewers(&doc);
	return 0;
}


