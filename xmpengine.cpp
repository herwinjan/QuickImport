#include "xmpengine.h"
#include <QDebug>
#include <QFileInfo>

XMPEngine::XMPEngine()
{
    QString filePath = "/Users/herwin/devel/test/_JS_HJS_001_TEST2.CR3";

    // Write XMP sidecar file
    // writeXmpSidecar(filePath);

    openXmpFile(filePath);
    dumpXmp();
    addXmpBag("Xmp.dc.subject", "Test");
    appendXmpBag("Xmp.dc.subject", "Testi2");
    dumpXmp();

    saveXmpFile();
}

void XMPEngine::writeXmpSidecar(const std::string &filePath)
{
    try {
        // Extract the directory and base filename, then append .xmp extension
        std::filesystem::path path(filePath);
        std::filesystem::path xmpFilePath = path.parent_path() / (path.stem().string() + ".xmp");
        QString keywords;

        if (std::filesystem::exists(xmpFilePath)) {
            // Open the image file
            Exiv2::Image::UniquePtr image = Exiv2::ImageFactory::open(xmpFilePath);
            image->readMetadata();

            // Get the existing XMP data
            xmpData = image->xmpData();
            if (!xmpData.empty()) {
                // Print existing XMP data
                std::cout << "Existing XMP data:" << std::endl;
                for (Exiv2::XmpData::const_iterator md = xmpData.begin(); md != xmpData.end();
                     ++md) {
                    std::cout << md->key() << " " << md->value() << std::endl;
                }

                // Check if the key exists

                // Exiv2::XmpData::iterator pos = xmpData.findKey(
                // Exiv2::XmpKey("Xmp.iptc.AltTextAccessibility"));
                // if (pos != xmpData.end())
                // xmpData.erase(pos);

                Exiv2::XmpData::iterator pos = xmpData.findKey(Exiv2::XmpKey("Xmp.dc.subject"));
                if (pos != xmpData.end()) {
                    keywords = QString::fromLocal8Bit(xmpData["Xmp.dc.subject"].toString());
                    qDebug() << keywords;
                    xmpData.erase(pos);
                }
            }
        }

        // Add a language alternative property
        std::unique_ptr<Exiv2::Value> v = Exiv2::Value::create(Exiv2::xmpText);

        v = Exiv2::Value::create(Exiv2::langAlt);

        v->read("lang=x-default Check, World"); // qualifier
        xmpData.add(Exiv2::XmpKey("Xmp.dc.description"), v.get());

        v = Exiv2::Value::create(Exiv2::xmpBag);

        v->read("Halo, Dit, is een, test, ");
        xmpData.add(Exiv2::XmpKey("Xmp.dc.subject"), v.get());
        // xmpData["Xmp.dc.subject"] = ""; // an array item
        // xmpData["Xmp.dc.subject"] = "Rubbertree"; // add a 2nd array item

        // Create the XMP packet
        std::string xmpPacket;
        if (Exiv2::XmpParser::encode(xmpPacket, xmpData) != 0) {
            throw Exiv2::Error(Exiv2::ErrorCode::kerErrorMessage, "Failed to encode XMP data");
        }

        // Write the XMP packet to a sidecar file
        std::ofstream xmpFile(xmpFilePath);
        if (!xmpFile.is_open()) {
            throw Exiv2::Error(Exiv2::ErrorCode::kerErrorMessage,
                               "Failed to open XMP file for writing");
        }
        xmpFile << xmpPacket;
        xmpFile.close();

        std::cout << "XMP sidecar file created at: " << xmpFilePath << std::endl;
    } catch (const Exiv2::Error &e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}

void XMPEngine::openXmpFile(QString file)
{
    // Exiv2::XmpParser::initialize();

    QFileInfo fi(file);
    fileName = fi.absolutePath() + "/" + fi.baseName() + ".xmp";

    qDebug() << fileName.toStdString();
    try {
        if (std::filesystem::exists(fileName.toStdString())) {
            // Open the image file
            Exiv2::Image::UniquePtr image = Exiv2::ImageFactory::open(fileName.toStdString());
            image->readMetadata();

            // Get the existing XMP data
            xmpData = image->xmpData();
        }
    } catch (const Exiv2::Error &e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}

void XMPEngine::saveXmpFile()
{
    // Create the XMP packet
    std::string xmpPacket;
    if (Exiv2::XmpParser::encode(xmpPacket, xmpData) != 0) {
        throw Exiv2::Error(Exiv2::ErrorCode::kerErrorMessage, "Failed to encode XMP data");
    }

    // Write the XMP packet to a sidecar file
    std::ofstream xmpFile(fileName.toStdString());
    if (!xmpFile.is_open()) {
        throw Exiv2::Error(Exiv2::ErrorCode::kerErrorMessage, "Failed to open XMP file for writing");
    }
    xmpFile << xmpPacket;
    xmpFile.close();
}

void XMPEngine::dumpXmp()
{
    if (!xmpData.empty()) {
        // Print existing XMP data
        std::cout << "Existing XMP data:" << std::endl;
        for (Exiv2::XmpData::const_iterator md = xmpData.begin(); md != xmpData.end(); ++md) {
            std::cout << md->key() << " " << md->value() << std::endl;
        }
    }
}
void XMPEngine::addLangAlt(QString key, QString value, bool _overwrite)
{
    std::unique_ptr<Exiv2::Value> v = Exiv2::Value::create(Exiv2::langAlt);
    v->read(value.toStdString());
    xmpData.add(Exiv2::XmpKey(key.toStdString()), v.get());
}
void XMPEngine::addXmpBag(QString key, QString value, bool _overwrite)
{
    std::unique_ptr<Exiv2::Value> v = Exiv2::Value::create(Exiv2::xmpBag);
    v->read(value.toStdString());
    xmpData.add(Exiv2::XmpKey(key.toStdString()), v.get());
}
void XMPEngine::appendXmpBag(QString key, QString value)
{
    QString keywords = QString::fromLocal8Bit(xmpData[key.toStdString()].toString());
    keywords = keywords.trimmed();
    if (keywords.endsWith(',')) {
        keywords.chop(1); // Remove the trailing comma
    }
    if (!keywords.isEmpty()) {
        keywords.append(", " + value);
    } else {
        keywords = value;
    }

    std::unique_ptr<Exiv2::Value> v = Exiv2::Value::create(Exiv2::xmpBag);
    v->read(keywords.toStdString());
    xmpData.add(Exiv2::XmpKey(key.toStdString()), v.get());
}
