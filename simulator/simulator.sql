/*
Navicat MySQL Data Transfer

Source Server         : apache
Source Server Version : 50717
Source Host           : 192.168.235.130:3306
Source Database       : test

Target Server Type    : MYSQL
Target Server Version : 50717
File Encoding         : 65001

Date: 2017-02-13 10:38:57
*/

SET FOREIGN_KEY_CHECKS=0;

-- ----------------------------
-- Table structure for `AP_Table`
-- ----------------------------
DROP TABLE IF EXISTS `AP_Table`;
CREATE TABLE `AP_Table` (
  `Index` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `Model` varchar(255) DEFAULT NULL,
  `Mac` varchar(255) DEFAULT NULL,
  `WTPname` varchar(255) DEFAULT NULL,
  `Hwversion` varchar(255) DEFAULT NULL,
  `Swversion` varchar(255) DEFAULT NULL,
  `BootVersion` varchar(255) DEFAULT NULL,
  `State` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `RadioCount` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `RadioInfoIndex` varchar(10) DEFAULT NULL,
  `ConnectRadioMac` varchar(255) DEFAULT NULL,
  `StaCount` mediumint(8) unsigned NOT NULL DEFAULT '0',
  `StaIndex` varchar(50) DEFAULT '0',
  `IsCPE` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `OnlineTimeStamp` bigint(20) unsigned NOT NULL DEFAULT '0',
  `WTP_PORT_CONTROL` int(10) unsigned NOT NULL DEFAULT '0',
  `WTP_PORT_DATA` int(10) unsigned NOT NULL DEFAULT '0',
  `CWDiscoveryCount` int(10) unsigned NOT NULL DEFAULT '0',
  `WTPSocket` int(10) unsigned NOT NULL DEFAULT '0',
  `WTPDataSocket` int(10) unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`Index`),
  KEY `Index` (`Index`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

-- ----------------------------
-- Records of AP_Table
-- ----------------------------

-- ----------------------------
-- Table structure for `Radio_Table`
-- ----------------------------
DROP TABLE IF EXISTS `Radio_Table`;
CREATE TABLE `Radio_Table` (
  `Index` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `PhyName` varchar(255) DEFAULT NULL,
  `Bssid` varchar(255) DEFAULT NULL,
  PRIMARY KEY (`Index`),
  KEY `Index` (`Index`) USING BTREE
) ENGINE=InnoDB AUTO_INCREMENT=3 DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

-- ----------------------------
-- Records of Radio_Table
-- ----------------------------

-- ----------------------------
-- Table structure for `Sta_Table`
-- ----------------------------
DROP TABLE IF EXISTS `Sta_Table`;
CREATE TABLE `Sta_Table` (
  `Index` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `IP` varchar(255) DEFAULT NULL,
  `Mac` varchar(255) DEFAULT NULL,
  `Type` varchar(255) DEFAULT NULL,
  `Rssi` char(255) NOT NULL DEFAULT '0',
  `Ssid` varchar(255) DEFAULT NULL,
  `WireMode` varchar(255) DEFAULT NULL,
  `OnlineTime` bigint(20) unsigned NOT NULL DEFAULT '0',
  `RadioId` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `WlanId` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `TxDataRate` char(255) NOT NULL DEFAULT '0',
  `RxDataRate` char(255) NOT NULL DEFAULT '0',
  `TxFlow` bigint(20) unsigned NOT NULL DEFAULT '0',
  `RxFlow` bigint(20) unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`Index`),
  KEY `Index` (`Index`) USING BTREE
) ENGINE=InnoDB AUTO_INCREMENT=3 DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

-- ----------------------------
-- Records of Sta_Table
-- ----------------------------
