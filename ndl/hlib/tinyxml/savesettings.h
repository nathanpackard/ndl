#ifndef SAVESETTINGSCLASS
#define SAVESETTINGSCLASS

#include <iostream>
#include <sstream>
#include "tinyxml.h"

class SaveSettings {
    public:
    SaveSettings(std::string filename){
        strncpy(m_filename,filename.c_str(),sizeof(m_filename));
    }
    template<class T>
    bool Save(std::string key, std::string attribute, T value){
        std::ostringstream strout;
        strout << value;
        return Save(key,attribute,strout.str());
    }
    bool Save(std::string key, std::string attribute, std::string value){
        TiXmlDocument doc(m_filename);
        TiXmlNode* node = doc.FirstChild( key );
        if (node){
            TiXmlElement* item = node->ToElement();
    		item->SetAttribute( attribute.c_str(), value.c_str() );
        } else {
            TiXmlElement item( key.c_str() );
            item.SetAttribute( attribute.c_str(), value.c_str() );
            doc.InsertEndChild( item );
        }
        doc.SaveFile();
        return true;
    }
    template<class T>
    bool Load(std::string key, std::string attribute, T& value){
        std::string strvalue;
        bool result = Load(key,attribute,strvalue);
        if (result){
            istringstream strin(strvalue);
            strin >> value;
            return true;
        } else return false;
    }
    bool Load(std::string key,std::string attribute, std::string& value ){
        TiXmlDocument doc(m_filename);
        TiXmlNode* node = doc.FirstChild( key.c_str() );
        if (!node) false;
        TiXmlElement* todoElement = node->ToElement();
        value = todoElement->Attribute(attribute.c_str());
        return true;
    }
    char m_filename[1000];
};

#endif