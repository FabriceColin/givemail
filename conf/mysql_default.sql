USE mysql;

--
-- Minimal user setup
-- Replace HOSTNAME with your host name
-- 

DELETE FROM user;
GRANT ALL ON *.*						TO root@"%"		IDENTIFIED BY "root" WITH GRANT OPTION;
GRANT ALL ON *.*						TO root@"HOSTNAME.%"	IDENTIFIED BY "root" WITH GRANT OPTION;
GRANT SELECT, INSERT, UPDATE, DELETE, CREATE, DROP ON *.*	TO dba@"%"		IDENTIFIED BY "dba";
GRANT SELECT, INSERT, UPDATE, DELETE, CREATE, DROP ON *.*	TO dba@"HOSTNAME.%"	IDENTIFIED BY "dba";

DROP DATABASE IF EXISTS MyGiveMailDB;
CREATE DATABASE MyGiveMailDB DEFAULT CHARSET=utf8;

GRANT SELECT, INSERT, UPDATE, DELETE, CREATE, DROP ON MyGiveMailDB.*       TO givemail@"%"              IDENTIFIED BY "givemail";
GRANT SELECT, INSERT, UPDATE, DELETE, CREATE, DROP ON MyGiveMailDB.*       TO givemail@"HOSTNAME.%"        IDENTIFIED BY "givemail";

FLUSH PRIVILEGES;

USE MyGiveMailDB;

--
-- Table structure for table `WebAPIKeys`
--

DROP TABLE IF EXISTS `WebAPIKeys`;
SET @saved_cs_client     = @@character_set_client;
SET character_set_client = utf8;
CREATE TABLE `WebAPIKeys` (
  `KeyID` VARCHAR(50) PRIMARY KEY,
  `ApplicationKeyID` VARCHAR(255),
  `KeyValue` VARCHAR(255),
  `CreationDate` INTEGER,
  `ExpiryDate` INTEGER
) ENGINE=MyISAM AUTO_INCREMENT=11 DEFAULT CHARSET=utf8;
SET character_set_client = @saved_cs_client;

--
-- Table structure for table `WebAPIUsage`
--

DROP TABLE IF EXISTS `WebAPIUsage`;
SET @saved_cs_client     = @@character_set_client;
SET character_set_client = utf8;
CREATE TABLE `WebAPIUsage` (
  `UsageID` VARCHAR(50) PRIMARY KEY,
  `TimeStamp` INTEGER, 
  `Status` INTEGER,
  `KeyID` INTEGER,
  `Hash` VARCHAR(255),
  `CallName` VARCHAR(255),
  `RemoteAddress` VARCHAR(255),
  `RemotePort` INTEGER
) ENGINE=MyISAM AUTO_INCREMENT=15 DEFAULT CHARSET=utf8;
SET character_set_client = @saved_cs_client;

--
-- Table structure for table `Campaigns`
--

DROP TABLE IF EXISTS `Campaigns`;
SET @saved_cs_client     = @@character_set_client;
SET character_set_client = utf8;
CREATE TABLE `Campaigns` (
  `CampaignID` VARCHAR(50) PRIMARY KEY,
  `CampaignName` VARCHAR(255),
  `Status` VARCHAR(255),
  `HtmlContent` TEXT,
  `PlainContent` TEXT,
  `PersonalisedHtml` INTEGER,
  `PersonalisedPlain` INTEGER,
  `Subject` VARCHAR(255),
  `FromName` VARCHAR(255),
  `FromEmailAddress` VARCHAR(255),
  `ReplyName` VARCHAR(255),
  `ReplyEmailAddress` VARCHAR(255),
  `ProcessingDate` INTEGER,
  `SenderEmailAddress` VARCHAR(255),
  `UnsubscribeLink` VARCHAR(255)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;
SET character_set_client = @saved_cs_client;

--
-- Table structure for table `Recipients`
--

DROP TABLE IF EXISTS `Recipients`;
SET @saved_cs_client     = @@character_set_client;
SET character_set_client = utf8;
CREATE TABLE `Recipients` (
  `RecipientID` VARCHAR(50) PRIMARY KEY,
  `CampaignID` VARCHAR(50),
  `RecipientName` VARCHAR(255),
  `Status` VARCHAR(255),
  `StatusCode` INTEGER,
  `EmailAddress` VARCHAR(255),
  `ReturnPath` VARCHAR(255),
  `DomainName` VARCHAR(255),
  `SendDate` INTEGER,
  `AttemptsCount` INTEGER
) ENGINE=MyISAM DEFAULT CHARSET=utf8;
SET character_set_client = @saved_cs_client;

--
-- Table structure for table `CustomFields`
--

DROP TABLE IF EXISTS `CustomFields`;
SET @saved_cs_client     = @@character_set_client;
SET character_set_client = utf8;
CREATE TABLE `CustomFields` (
  `RecipientID` VARCHAR(50),
  `CustomFieldName` VARCHAR(255),
  `CustomFieldValue` VARCHAR(255),
  PRIMARY KEY(RecipientID, CustomFieldName)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;
SET character_set_client = @saved_cs_client;

--
-- Table structure for table `Attachments`
--

DROP TABLE IF EXISTS `Attachments`;
SET @saved_cs_client     = @@character_set_client;
SET character_set_client = utf8;
CREATE TABLE `Attachments` (
  `CampaignID` VARCHAR(50),
  `AttachmentID` VARCHAR(255),
  `AttachmentValue` VARCHAR(255),
  `AttachmentType` VARCHAR(255),
  PRIMARY KEY(CampaignID, AttachmentID)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;
SET character_set_client = @saved_cs_client;

